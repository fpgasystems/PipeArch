#pragma once


#include "Instruction.h"
#include "FPGA_ColumnML.h"

class GlmMachine : public iFPGA {
public:
	GlmMachine(const char* accel_uuid) : iFPGA(accel_uuid) {}

private:
	shared_buffer::ptr_t StartProgram(Instruction inst[], uint32_t numInstructions, volatile float* input, volatile float* output, uint32_t whichInstance) {

		std::vector<Instruction> instructions;
		for (uint32_t i = 0; i < numInstructions; i++) {
			instructions.push_back(inst[i]);
		}

		// Copy program to FPGA memory
		shared_buffer::ptr_t programMemoryHandle = iFPGA::malloc(instructions.size()*64);
		auto programMemory = reinterpret_cast<volatile uint32_t*>(programMemoryHandle->c_type());
		uint32_t k = 0;
		for (Instruction i: instructions) {
			i.LoadInstruction(programMemory + k*Instruction::NUM_WORDS);
			k++;
		}

		uint32_t vc_select = 0;
		output[0] = 0;
		iFPGA::writeCSR(whichInstance*4 + 0, intptr_t(input));
		iFPGA::writeCSR(whichInstance*4 + 1, intptr_t(output));
		iFPGA::writeCSR(whichInstance*4 + 2, intptr_t(programMemory));
		iFPGA::writeCSR(whichInstance*4 + 3, (vc_select << 16) | (uint32_t)instructions.size());

		return programMemoryHandle;
	}

public:
	void JoinProgram(shared_buffer::ptr_t& outputHandle, shared_buffer::ptr_t& programMemoryHandle) {

		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (iFPGA::hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 100;

		double start = get_time();

		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};

		double end = get_time();

		cout << "Time: " << end-start << endl;

		printMPF();

		if (programMemoryHandle != NULL) {
			programMemoryHandle->release();
		}
	}

	shared_buffer::ptr_t fSGD(
		FPGA_ColumnML& cML,
		ModelType type,
		uint32_t numEpochs,
		float stepSize,
		float lambda,
		AdditionalArguments* args)
	{
		if (cML.m_memory == nullptr) {
			cout << "cML.m_memory is nullptr!" << endl;
			exit(1);
		}

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		AccessProperties accessModel(5);
		accessModel.Set(2, modelOffsetInBRAM, cML.m_modelChunk.m_lengthInCL);

		AccessProperties accessLabels(5);
		accessLabels.Set(3, labelOffsetInBRAM, cML.m_partitionSizeInCL);

		AccessProperties accessSamples(5);
		accessSamples.Set(0, 0, cML.m_numFeaturesInCL);
		accessSamples.Set(1, 0, cML.m_numFeaturesInCL);

		AccessProperties writebackModel(2);
		writebackModel.Set(0, modelOffsetInBRAM, cML.m_numFeaturesInCL);

		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		// Load model
		inst[pc].Load(cML.m_modelChunk.m_offsetInCL, cML.m_modelChunk.m_lengthInCL, 0, 0, 0, accessModel);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].ResetIndex(2);
		pc++;

		uint32_t beginEpoch = pc;
		// Load labels in partition
		inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_partitionSizeInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
		pc++;

		inst[pc].Prefetch(cML.m_samplesChunk.m_offsetInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, cML.m_partitionSize*cML.m_numFeaturesInCL, 0);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(cML.m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		pc++;

		// Start---Innermost loop
		inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		inst[pc].MakeNonBlocking();
		uint32_t pcModify = pc;
		pc++;

		inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, true);
		inst[pc].MakeNonBlocking();
		inst[pc].IncrementIndex(0);
		pc++;

		inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(cML.m_numFeaturesInCL, true, false, modelOffsetInBRAM, 0xFFFF);
		inst[pc].Jump(0, cML.m_partitionSize-1, pcModify, pc+1);
		pc++;
		// End---Innermost loop

		inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, false);
		inst[pc].Jump(1, cML.m_numPartitions-1, beginEpoch, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		if ( cML.m_rest > 1 ) {
			accessLabels.Set(3, labelOffsetInBRAM, cML.m_restInCL);
			inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_restInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
			pc++;

			inst[pc].Prefetch(cML.m_samplesChunk.m_offsetInCL, cML.m_rest*cML.m_numFeaturesInCL, 0, cML.m_partitionSize*cML.m_numFeaturesInCL, 0);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Dot(cML.m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
			pc++;

			// Start---Innermost loop
			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			inst[pc].MakeNonBlocking();
			uint32_t pcRestSamples = pc;
			pc++;

			inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, true);
			inst[pc].MakeNonBlocking();
			inst[pc].IncrementIndex(0);
			pc++;

			inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Dot(cML.m_numFeaturesInCL, true, false, modelOffsetInBRAM, 0xFFFF);
			inst[pc].Jump(0, cML.m_rest-1, pcRestSamples, pc+1);
			pc++;
			// End---Innermost loop

			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, false);
			pc++;
		}

		// WriteBack
		inst[pc].WriteBack(false, 1, cML.m_numFeaturesInCL,
			0, 0, cML.m_numFeaturesInCL,
			0, false, writebackModel);
		pc++;

		inst[pc].Jump(2, numEpochs-1, beginEpoch, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		if (cML.m_outputHandle != NULL) {
			cML.m_outputHandle->release();
			cML.m_outputHandle = NULL;
		}
		cML.m_outputHandle = iFPGA::malloc((numEpochs*cML.m_numFeaturesInCL+1)*64);
		auto output = reinterpret_cast<volatile float*>(cML.m_outputHandle->c_type());
		assert(NULL != output);

		auto handle = StartProgram(inst, pc, cML.m_memory, output, 0);

		return handle;
	}


};