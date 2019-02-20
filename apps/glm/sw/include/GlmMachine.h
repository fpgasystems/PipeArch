#pragma once

#include <mutex>

#include "Instruction.h"
#include "FPGA_ColumnML.h"

struct ResultHandle
{
	shared_buffer::ptr_t m_programMemoryHandle;
	shared_buffer::ptr_t m_outputHandle;
	uint32_t m_instanceId;
};

class GlmMachine : public iFPGA {
private:
	static const uint32_t MAX_MEMORY_SIZE = (1 << 10)*16;
	static const uint32_t NUM_INSTANCES = 1;

	std::mutex m_mutex;
	uint32_t m_numJobsRunning[NUM_INSTANCES];


	shared_buffer::ptr_t StartProgram(Instruction inst[], uint32_t numInstructions, shared_buffer::ptr_t inputHandle, shared_buffer::ptr_t outputHandle, uint32_t whichInstance) {

		std::vector<Instruction> instructions;
		for (uint32_t i = 0; i < numInstructions; i++) {
			instructions.push_back(inst[i]);
		}

		// Copy program to FPGA memory
		shared_buffer::ptr_t programMemoryHandle = iFPGA::malloc(instructions.size()*64);
		auto programMemory = reinterpret_cast<volatile uint32_t*>(programMemoryHandle->c_type());
		assert(NULL != programMemory);
		uint32_t k = 0;
		for (Instruction i: instructions) {
			i.LoadInstruction(programMemory + k*Instruction::NUM_WORDS);
			k++;
		}

		auto inputMemory = reinterpret_cast<volatile uint32_t*>(inputHandle->c_type());
		assert(NULL != inputMemory);
		auto outputMemory = reinterpret_cast<volatile uint32_t*>(outputHandle->c_type());
		assert(NULL != outputMemory);

		m_mutex.lock();

		uint32_t whichThread = m_numJobsRunning[whichInstance];
		uint32_t vc_select = 0;
		outputMemory[0] = 0;
		iFPGA::writeCSR(whichInstance*4 + 0, (vc_select << 30) | (whichThread << 16) | (instructions.size() & 0xFFFF) );
		iFPGA::writeCSR(whichInstance*4 + 1, intptr_t(programMemory));
		iFPGA::writeCSR(whichInstance*4 + 2, intptr_t(inputMemory));
		iFPGA::writeCSR(whichInstance*4 + 3, intptr_t(outputMemory));

		m_numJobsRunning[whichInstance]++;

		m_mutex.unlock();

		return programMemoryHandle;
	}

public:

	GlmMachine(const char* accel_uuid) : iFPGA(accel_uuid) {
		for (uint32_t i = 0; i < NUM_INSTANCES; i++) {
			m_numJobsRunning[i] = 0;
		}
	}

	void JoinProgram(ResultHandle& handle) {

		auto output = reinterpret_cast<volatile float*>(handle.m_outputHandle->c_type());
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

		m_mutex.lock();
		m_numJobsRunning[handle.m_instanceId]++;
		m_mutex.unlock();

		if (handle.m_programMemoryHandle != NULL) {
			handle.m_programMemoryHandle->release();
		}
	}

	ResultHandle fSGD(
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

		ResultHandle resultHandle;

		resultHandle.m_outputHandle = iFPGA::malloc((numEpochs*cML.m_numFeaturesInCL+1)*64);
		resultHandle.m_programMemoryHandle = StartProgram(inst, pc, cML.m_handle, resultHandle.m_outputHandle, 0);

		return resultHandle;
	}

	ResultHandle fSGD_minibatch(
		FPGA_ColumnML& cML,
		ModelType type,
		uint32_t numEpochs,
		uint32_t minibatchSize, 
		float stepSize,
		float lambda,
		AdditionalArguments* args)
	{
		if (cML.m_memory == nullptr) {
			cout << "cML.m_memory is nullptr!" << endl;
			exit(1);
		}
		if (cML.m_partitionSize%minibatchSize > 0) {
			cout << "cML.m_partitionSize%minibatchSize = 0 must hold!" << endl;
			exit(1);
		}
		if (cML.m_partitionSize/minibatchSize == 1) {
			cout << "cML.m_partitionSize/minibatchSize > 1 must hold!" << endl;
			exit(1);
		}

		float scaledStepSize = stepSize/minibatchSize;

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		AccessProperties accessModel(5);
		accessModel.Set(2, modelOffsetInBRAM, cML.m_modelChunk.m_lengthInCL);

		AccessProperties accessLabels(5);
		accessLabels.Set(3, labelOffsetInBRAM, cML.m_partitionSizeInCL);

		AccessProperties minibatchAccessSamples(5);
		minibatchAccessSamples.Set(0, 0, minibatchSize*cML.m_numFeaturesInCL);
		minibatchAccessSamples.Set(1, 0, minibatchSize*cML.m_numFeaturesInCL);

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

		// Load labels in partition
		inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_partitionSizeInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
		uint32_t pcLabels = pc;
		pc++;

		inst[pc].Prefetch(cML.m_samplesChunk.m_offsetInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, cML.m_partitionSize*cML.m_numFeaturesInCL, 0);
		inst[pc].MakeNonBlocking();
		pc++;

		// Start---Innermost loop
		inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, minibatchSize*cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_partitionSize*cML.m_numFeaturesInCL, 0, minibatchAccessSamples);
		uint32_t pcSamples = pc;
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, cML.m_numFeaturesInCL);
		// inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(minibatchSize, cML.m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Modify(minibatchSize, labelOffsetInBRAM, type, 0, scaledStepSize, lambda);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Update(minibatchSize, modelOffsetInBRAM, cML.m_numFeaturesInCL, false);
		inst[pc].IncrementIndex(0, minibatchSize);
		inst[pc].Jump(0, cML.m_partitionSize-minibatchSize, pcSamples, pc+2);
		pc++;
		// End---Innermost loop

		inst[pc].Jump(1, cML.m_numPartitions-1, pcLabels, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		if ( cML.m_rest > 0 ) {
			accessLabels.Set(3, labelOffsetInBRAM, cML.m_restInCL);
			inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_restInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
			pc++;

			// Start---Innermost loop
			inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL*cML.m_partitionSize, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			uint32_t pcRestSamples = pc;
			pc++;

			inst[pc].Dot(cML.m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
			pc++;

			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			pc++;

			inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, false);
			inst[pc].Jump(0, cML.m_rest-1, pcRestSamples, pc+1);
			inst[pc].IncrementIndex(0);
			pc++;
			// End---Innermost loop
		}

		// WriteBack
		inst[pc].WriteBack(false, 1, cML.m_numFeaturesInCL,
			0, 0, cML.m_numFeaturesInCL,
			0, false, writebackModel);
		pc++;

		inst[pc].Jump(2, numEpochs-1, pcLabels, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		ResultHandle resultHandle;

		resultHandle.m_outputHandle = iFPGA::malloc((numEpochs*cML.m_numFeaturesInCL+1)*64);
		resultHandle.m_programMemoryHandle = StartProgram(inst, pc, cML.m_handle, resultHandle.m_outputHandle, 0);

		return resultHandle;
	}

	ResultHandle fSCD(
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

		float scaledStepSize = stepSize/cML.m_partitionSize;
		float scaledLambda = stepSize*lambda;

		uint32_t residualOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;
		uint32_t modelOffsetInBRAM = labelOffsetInBRAM + cML.m_partitionSizeInCL;
		uint32_t accesspropsOffsetInBRAM = 0;

		AccessProperties accessResidual(5);
		accessResidual.Set(2, residualOffsetInBRAM, cML.m_partitionSizeInCL);

		AccessProperties accessLabels(5);
		accessLabels.Set(3, labelOffsetInBRAM, cML.m_partitionSizeInCL);

		AccessProperties accessModel(5);
		accessModel.Set(3, modelOffsetInBRAM, cML.m_numFeaturesInCL);

		AccessProperties accessSamples(5);
		accessSamples.Set(0, 0, cML.m_partitionSizeInCL);
		accessSamples.Set(1, 0, cML.m_partitionSizeInCL);

		AccessProperties accessAccessProps(5);
		accessAccessProps.Set(4, accesspropsOffsetInBRAM, cML.m_numFeaturesInCL*2);

		AccessProperties writebackResidual(2);
		writebackResidual.Set(0, residualOffsetInBRAM, cML.m_partitionSizeInCL);

		AccessProperties writebackModel(2);
		writebackModel.Set(1, modelOffsetInBRAM, cML.m_numFeaturesInCL);


		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		// Load residual
		inst[pc].Load(cML.m_residualChunk.m_offsetInCL, cML.m_partitionSizeInCL, 0, cML.m_partitionSizeInCL, 0, accessResidual);
		pc++;

		// Load labels in partition
		inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_partitionSizeInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
		pc++;

		// Load model
		inst[pc].Load(cML.m_modelChunk.m_offsetInCL, cML.m_numFeaturesInCL, 0, cML.m_numFeaturesInCL, 0, accessModel);
		pc++;

		// Load accessprops
		inst[pc].Load(cML.m_accesspropsChunk.m_offsetInCL, cML.m_numFeaturesInCL*2, 0, cML.m_numFeaturesInCL*2, 0, accessAccessProps);
		pc++;

		// Load samples
		inst[pc].LocalLoad(0, 1, 0, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(cML.m_partitionSizeInCL, false, true, residualOffsetInBRAM, labelOffsetInBRAM);
		pc++;

		// Start---Innermost loop
		inst[pc].Modify(modelOffsetInBRAM, type, 1, scaledStepSize, scaledLambda);
		inst[pc].MakeNonBlocking();
		uint32_t pcModify = pc;
		pc++;

		inst[pc].Update(residualOffsetInBRAM, cML.m_partitionSizeInCL, true);
		inst[pc].IncrementIndex(0);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].LocalLoad(0, 1, 0, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(cML.m_partitionSizeInCL, true, true, residualOffsetInBRAM, labelOffsetInBRAM);
		inst[pc].Jump(0, cML.m_cstore->m_numFeatures-1, pcModify, pc+1);
		pc++;
		// End---Innermost loop

		inst[pc].Modify(modelOffsetInBRAM, type, 1, scaledStepSize, scaledLambda);
		pc++;

		inst[pc].Update(residualOffsetInBRAM, cML.m_partitionSizeInCL, false);
		pc++;

		inst[pc].WriteBack(true, cML.m_residualChunk.m_offsetInCL, cML.m_partitionSizeInCL,
			0, cML.m_partitionSizeInCL, 0,
			0, true, writebackResidual);
		pc++;

		inst[pc].WriteBack(true, cML.m_modelChunk.m_offsetInCL, cML.m_numFeaturesInCL,
			0, cML.m_numFeaturesInCL, 0,
			1, true, writebackModel);
		inst[pc].Jump(1, cML.m_numPartitions-1, 0, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		inst[pc].Jump(2, numEpochs-1, 0, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;
		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		ResultHandle resultHandle;

		resultHandle.m_outputHandle = iFPGA::malloc(64);
		resultHandle.m_programMemoryHandle = StartProgram(inst, pc, cML.m_handle, resultHandle.m_outputHandle, 0);

		return resultHandle;
	}

	ResultHandle fSGD_blocking(
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

		// Load labels in partition
		inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_partitionSizeInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
		uint32_t pcLabels = pc;
		pc++;

		// Start---Innermost loop
		inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL*cML.m_partitionSize, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		uint32_t pcSamples = pc;
		pc++;

		inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, cML.m_numFeaturesInCL);
		pc++;

		inst[pc].Dot(cML.m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		pc++;

		inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		pc++;

		inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, false);
		inst[pc].Jump(0, cML.m_partitionSize-1, pcSamples, pc+1);
		inst[pc].IncrementIndex(0);
		pc++;
		// End---Innermost loop

		inst[pc].Jump(1, cML.m_numPartitions-1, pcLabels, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		if ( cML.m_rest > 0 ) {
			accessLabels.Set(3, labelOffsetInBRAM, cML.m_restInCL);
			inst[pc].Load(cML.m_labelsChunk.m_offsetInCL, cML.m_restInCL, 0, cML.m_partitionSizeInCL, 0, accessLabels);
			pc++;

			// Start---Innermost loop
			inst[pc].Load(cML.m_samplesChunk.m_offsetInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL, cML.m_numFeaturesInCL*cML.m_partitionSize, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			uint32_t pcRestSamples = pc;
			pc++;

			inst[pc].Dot(cML.m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
			pc++;

			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			pc++;

			inst[pc].Update(modelOffsetInBRAM, cML.m_numFeaturesInCL, false);
			inst[pc].Jump(0, cML.m_rest-1, pcRestSamples, pc+1);
			inst[pc].IncrementIndex(0);
			pc++;
			// End---Innermost loop
		}

		// WriteBack
		inst[pc].WriteBack(false, 1, cML.m_numFeaturesInCL,
			0, 0, cML.m_numFeaturesInCL,
			0, false, writebackModel);
		pc++;

		inst[pc].Jump(2, numEpochs-1, pcLabels, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		ResultHandle resultHandle;

		resultHandle.m_outputHandle = iFPGA::malloc((numEpochs*cML.m_numFeaturesInCL+1)*64);
		resultHandle.m_programMemoryHandle = StartProgram(inst, pc, cML.m_handle, resultHandle.m_outputHandle, 0);

		return resultHandle;
	}

	void ReadBandwidth(uint32_t numIterations) {

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		uint32_t numLines = 2048;

		AccessProperties accessSamples(5);
		accessSamples.Set(2, 0, numLines);

		uint32_t pc = 0;

		inst[pc].Prefetch(0, numIterations*numLines, 0, 0, 0);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Load(0, numLines, 0, 0, 0, accessSamples);
		uint32_t pcLoad = pc;
		pc++;

		inst[pc].Jump(2, numIterations-1, pcLoad, 0xFFFFFFFF);
		inst[pc].IncrementIndex(2);
		pc++;

		auto inputHandle = iFPGA::malloc(numIterations*numLines*64);

		ResultHandle resultHandle;
		resultHandle.m_outputHandle = iFPGA::malloc(64);
		resultHandle.m_programMemoryHandle = StartProgram(inst, pc, inputHandle, resultHandle.m_outputHandle, 0);

		JoinProgram(resultHandle);
	}

	void Correctness() {

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		uint32_t numLines = 4;

		AccessProperties accessRead(5);
		accessRead.Set(2, 0, numLines);

		AccessProperties accessWriteback(2);
		accessWriteback.Set(0, 0, numLines);

		uint32_t pc = 0;

		// inst[pc].Prefetch(0, numLines, 0, 0, 0);
		// pc++;

		inst[pc].Load(0, numLines, 0, 0, 0, accessRead);
		uint32_t pcLoad = pc;
		pc++;

		inst[pc].WriteBack(0, 1, numLines, 0, 0, 0, 0, true, accessWriteback);
		pc++;

		inst[pc].Jump(2, 0, pcLoad, 0xFFFFFFFF);
		pc++;

		auto inputHandle = iFPGA::malloc(numLines*64);
		auto input = reinterpret_cast<volatile int*>(inputHandle->c_type());
		assert(NULL != input);

		for (uint32_t i = 0; i < numLines*16; i++) {
			input[i] = i+1;
		}

		ResultHandle resultHandle;

		resultHandle.m_outputHandle = iFPGA::malloc((numLines+1)*64);
		auto output = reinterpret_cast<volatile int*>(resultHandle.m_outputHandle->c_type());
		assert(NULL != output);

		for (uint32_t i = 0; i < (numLines+1)*16; i++) {
			output[i] = 0;
		}

		resultHandle.m_programMemoryHandle = StartProgram(inst, pc, inputHandle, resultHandle.m_outputHandle, 0);
		JoinProgram(resultHandle);

		for (uint32_t i = 0; i < numLines*16; i++) {
			cout << "input[" << i << "]: " << input[i] << endl;
		}
		for (uint32_t i = 0; i < (numLines+1)*16; i++) {
			cout << "output[" << i << "]: " << output[i] << endl;
		}

		bool pass = true;
		for (uint32_t i = 0; i < numLines*16; i++) {
			if (input[i] != output[16+i]) {
				pass = false;
				cout << "Missmatch at " << i << ". output[" << 16+i << "]: " << output[16+i] << endl;
			}
		}
		if (pass) {
			cout << "PASS!" << endl;
		}
	}

};