#include "ColumnML.h"
#include "iFPGA.h"

enum MemoryFormat {FormatSGD, FormatSCD};

class FPGA_ColumnML : public iFPGA, public ColumnML {

public:
	FPGA_ColumnML(const char* accel_uuid) : iFPGA(accel_uuid) {}

	volatile float* m_memory = nullptr;
	volatile float* m_model = nullptr;
	volatile float* m_labels = nullptr;
	volatile float* m_samples = nullptr;
	volatile float* m_residual = nullptr;

	uint32_t m_numSamplesInCL;
	uint32_t m_numFeaturesInCL;
	uint32_t m_alignedNumSamples;
	uint32_t m_alignedNumFeatures;
	uint32_t m_partitionSize;
	uint32_t m_partitionSizeInCL;
	uint32_t m_alignedPartitionSize;
	uint32_t m_numPartitions;
	uint32_t m_rest;
	uint32_t m_restInCL;

	access_t m_modelChunk;
	access_t m_labelChunk;
	access_t m_samplesChunk;
	access_t m_residualChunk;

	MemoryFormat m_currentMemoryFormat;

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize) {
		m_currentMemoryFormat = format;

		m_numSamplesInCL = (m_cstore->m_numSamples >> 4) + ((m_cstore->m_numSamples&0xF) > 0);
		m_numFeaturesInCL = (m_cstore->m_numFeatures >> 4) + ((m_cstore->m_numFeatures&0xF) > 0);
		m_partitionSize = (partitionSize > m_cstore->m_numSamples) ? m_cstore->m_numSamples : partitionSize;
		m_partitionSizeInCL = (m_partitionSize >> 4) + ((m_partitionSize & 0xF) > 0);
		m_alignedNumSamples = m_numSamplesInCL*16;
		m_alignedNumFeatures = m_numFeaturesInCL*16;
		m_alignedPartitionSize = m_partitionSizeInCL*16;
		m_numPartitions = m_cstore->m_numSamples/m_partitionSize;
		m_rest = m_cstore->m_numSamples % m_partitionSize;
		m_restInCL = (m_rest >> 4) + ((m_rest&0xF) > 0);

		std::cout << "m_numSamplesInCL: " << m_numSamplesInCL << std::endl;
		std::cout << "m_numFeaturesInCL: " << m_numFeaturesInCL << std::endl;
		std::cout << "m_partitionSize: " << m_partitionSize << std::endl;
		std::cout << "m_partitionSizeInCL: " << m_partitionSizeInCL << std::endl;
		std::cout << "m_alignedNumSamples: " << m_alignedNumSamples << std::endl;
		std::cout << "m_alignedNumFeatures: " << m_alignedNumFeatures << std::endl;
		std::cout << "m_alignedPartitionSize: " << m_alignedPartitionSize << std::endl;
		std::cout << "m_numPartitions: " << m_numPartitions << std::endl;
		std::cout << "m_rest: " << m_rest << std::endl;
		std::cout << "m_restInCL: " << m_restInCL << std::endl;

		uint32_t countCL = 0;

		if (format == FormatSGD) {
			// Model
			m_modelChunk.m_offsetInCL = countCL;
			countCL += m_numFeaturesInCL;
			m_modelChunk.m_lengthInCL = countCL - m_modelChunk.m_offsetInCL;

			// Labels
			m_labelChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_labelChunk.m_lengthInCL = countCL - m_labelChunk.m_offsetInCL;

			// Samples
			m_samplesChunk.m_offsetInCL = countCL;
			countCL += m_cstore->m_numSamples*m_numFeaturesInCL;
			m_samplesChunk.m_lengthInCL = countCL - m_samplesChunk.m_offsetInCL;

			// No residual used
			m_residualChunk.m_offsetInCL = 0;
			m_residualChunk.m_lengthInCL = 0;
		}
		else if (format == FormatSCD) {
			// Residual
			m_residualChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_residualChunk.m_lengthInCL = countCL - m_residualChunk.m_offsetInCL;

			// Labels
			m_labelChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_labelChunk.m_lengthInCL = countCL - m_labelChunk.m_offsetInCL;

			// Samples
			m_samplesChunk.m_offsetInCL = countCL;
			countCL += m_cstore->m_numFeatures*m_numSamplesInCL;
			m_samplesChunk.m_lengthInCL = countCL - m_samplesChunk.m_offsetInCL;

			// Model
			m_modelChunk.m_offsetInCL = countCL;
			countCL += m_numPartitions*m_numFeaturesInCL;
			m_modelChunk.m_lengthInCL = countCL - m_modelChunk.m_offsetInCL;
		}

		if (m_handle != NULL) {
			m_handle->release();
			m_handle = NULL;
		}
		m_handle = m_fpga->allocBuffer(countCL*64);
		m_memory = reinterpret_cast<volatile float*>(m_handle->c_type());
		assert(NULL != m_memory);

		memset((void*)m_memory, 0, 16*countCL*sizeof(float));

		m_model = m_memory + m_modelChunk.m_offsetInCL*16;
		m_residual = m_memory + m_residualChunk.m_offsetInCL*16;
		m_labels = m_memory + m_labelChunk.m_offsetInCL*16;
		m_samples = m_memory + m_samplesChunk.m_offsetInCL*16;

		if (format == FormatSGD) {
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				m_model[j] = 0;
			}
			for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
				m_labels[i] = m_cstore->m_labels[i];
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					m_samples[i*m_alignedNumFeatures + j] = m_cstore->m_samples[j][i];
				}
			}
		}
		else if (format == FormatSCD) {
			for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
				m_residual[i] = 0;
				m_labels[i] = m_cstore->m_labels[i];
			}
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
					m_samples[j*m_alignedNumSamples + i] = m_cstore->m_samples[j][i];
				}
			}
			for (uint32_t p = 0; p < m_numPartitions; p++) {
				for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
					m_model[p*m_alignedNumFeatures + j] = 0;
				}
			}
		}

		return countCL;
	}

	void CopyDataToFPGAMemory(MemoryFormat format, uint32_t partitionSize) {
		uint32_t countCL = CreateMemoryLayout(format, partitionSize);
		cout << "CopyDataToFPGAMemory END" << endl;
	}

	void RunProgram(Instruction inst[], uint32_t numInstructions, volatile float* output) {

		std::vector<Instruction> instructions;
		for (uint32_t i = 0; i < numInstructions; i++) {
			instructions.push_back(inst[i]);
		}

		// Copy program to FPGA memory
		auto programMemoryHandle = m_fpga->allocBuffer(instructions.size()*64);
		auto programMemory = reinterpret_cast<volatile uint32_t*>(programMemoryHandle->c_type());
		uint32_t k = 0;
		for (Instruction i: instructions) {
			i.Copy(programMemory + k*Instruction::NUM_WORDS);
			k++;
		}

		uint32_t vc_select = 0;
		m_csrs->writeCSR(0, intptr_t(m_memory));
		m_csrs->writeCSR(1, intptr_t(output));
		m_csrs->writeCSR(2, intptr_t(programMemory));
		m_csrs->writeCSR(3, (vc_select << 16) | (uint32_t)instructions.size());

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (m_fpga->hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 100;

		output[0] = 0;
		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};

		// Reads CSRs to get some statistics
		cout	<< "# List length: " << m_csrs->readCSR(0) << endl
				<< "# Linked list data entries read: " << m_csrs->readCSR(1) << endl;

		cout	<< "#" << endl
				<< "# AFU frequency: " << m_csrs->getAFUMHz() << " MHz"
				<< (m_fpga->hwIsSimulated() ? " [simulated]" : "")
				<< endl;

		// MPF VTP (virtual to physical) statistics
		mpf_handle::ptr_t mpf = m_fpga->mpf;
		if (mpfVtpIsAvailable(*mpf))
		{
			mpf_vtp_stats vtp_stats;
			mpfVtpGetStats(*mpf, &vtp_stats);

			cout << "#" << endl;
			if (vtp_stats.numFailedTranslations)
			{
				cout << "# VTP failed translating VA: 0x" << hex << uint64_t(vtp_stats.ptWalkLastVAddr) << dec << endl;
			}
			cout	<< "# VTP PT walk cycles: " << vtp_stats.numPTWalkBusyCycles << endl
					<< "# VTP L2 4KB hit / miss: " << vtp_stats.numTLBHits4KB << " / "
					<< vtp_stats.numTLBMisses4KB << endl
					<< "# VTP L2 2MB hit / miss: " << vtp_stats.numTLBHits2MB << " / "
					<< vtp_stats.numTLBMisses2MB << endl;
		}
	}

	void fSGD(
		ModelType type,
		float* xHistory,
		uint32_t numEpochs,
		float stepSize,
		float lambda,
		AdditionalArguments* args)
	{
		if (m_memory == nullptr) {
			cout << "m_memory is nullptr!" << endl;
			exit(1);
		}

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		AccessProperties accessModel(4);
		accessModel.Set(2, modelOffsetInBRAM, m_modelChunk.m_lengthInCL);

		AccessProperties accessLabels(4);
		accessLabels.Set(3, labelOffsetInBRAM, m_partitionSizeInCL);

		AccessProperties accessSamples(4);
		accessSamples.Set(0, 0, m_numFeaturesInCL);
		accessSamples.Set(1, 0, m_numFeaturesInCL);

		AccessProperties writebackModel(2);
		writebackModel.Set(0, modelOffsetInBRAM, m_numFeaturesInCL);

		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		// Load model
		inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, accessModel);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].ResetIndex(2);
		pc++;

		uint32_t beginEpoch = pc;
		// Load labels in partition
		inst[pc].Load(m_labelChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessLabels);
		pc++;

		inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		pc++;

		// Start---Innermost loop
		inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		inst[pc].MakeNonBlocking();
		uint32_t pcModify = pc;
		pc++;

		inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, true);
		inst[pc].MakeNonBlocking();
		inst[pc].IncrementIndex(0);
		pc++;

		inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(m_numFeaturesInCL, true, false, modelOffsetInBRAM, 0xFFFF);
		inst[pc].AddJump(0, m_partitionSize-1, pcModify, pc+1);
		pc++;
		// End---Innermost loop

		inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
		inst[pc].AddJump(1, m_numPartitions-1, beginEpoch, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		if ( m_rest > 1 ) {
			accessLabels.Set(3, labelOffsetInBRAM, m_restInCL);
			inst[pc].Load( m_labelChunk.m_offsetInCL, m_restInCL, 0, m_partitionSizeInCL, 0, accessLabels);
			pc++;

			inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
			pc++;

			// Start---Innermost loop
			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			inst[pc].MakeNonBlocking();
			uint32_t pcRestSamples = pc;
			pc++;

			inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, true);
			inst[pc].MakeNonBlocking();
			inst[pc].IncrementIndex(0);
			pc++;

			inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Dot(m_numFeaturesInCL, true, false, modelOffsetInBRAM, 0xFFFF);
			inst[pc].AddJump(0, m_rest-1, pcRestSamples, pc+1);
			pc++;
			// End---Innermost loop

			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			inst[pc].MakeNonBlocking();
			pc++;

			inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
			pc++;
		}

		// WriteBack
		inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
			0, 0, m_numFeaturesInCL,
			0, false, writebackModel);
		pc++;

		inst[pc].AddJump(2, numEpochs-1, beginEpoch, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		auto outputHandle = m_fpga->allocBuffer((numEpochs*m_numFeaturesInCL+1)*64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

		RunProgram(inst, pc, output);

		// Verify
		xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = Loss(type, xHistory + e*m_alignedNumFeatures, lambda, args);
			std::cout << "loss " << e << ": " << loss << std::endl;
		}
	}

	void fSGD_blocking(
		ModelType type,
		float* xHistory,
		uint32_t numEpochs,
		float stepSize,
		float lambda,
		AdditionalArguments* args)
	{
		if (m_memory == nullptr) {
			cout << "m_memory is nullptr!" << endl;
			exit(1);
		}

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		AccessProperties accessModel(4);
		accessModel.Set(2, modelOffsetInBRAM, m_modelChunk.m_lengthInCL);

		AccessProperties accessLabels(4);
		accessLabels.Set(3, labelOffsetInBRAM, m_partitionSizeInCL);

		AccessProperties accessSamples(4);
		accessSamples.Set(0, 0, m_numFeaturesInCL);
		accessSamples.Set(1, 0, m_numFeaturesInCL);

		AccessProperties writebackModel(2);
		writebackModel.Set(0, modelOffsetInBRAM, m_numFeaturesInCL);

		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		// Load model
		inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, accessModel);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].ResetIndex(2);
		pc++;

		// Load labels in partition
		inst[pc].Load(m_labelChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessLabels);
		uint32_t pcLabels = pc;
		pc++;

		// Start---Innermost loop
		inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_numFeaturesInCL*m_partitionSize, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		uint32_t pcSamples = pc;
		pc++;

		inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		pc++;

		inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		pc++;

		inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
		inst[pc].AddJump(0, m_partitionSize-1, pcSamples, pc+1);
		inst[pc].IncrementIndex(0);
		pc++;
		// End---Innermost loop

		inst[pc].AddJump(1, m_numPartitions-1, pcLabels, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		if ( m_rest > 0 ) {
			accessLabels.Set(3, labelOffsetInBRAM, m_restInCL);
			inst[pc].Load( m_labelChunk.m_offsetInCL, m_restInCL, 0, m_partitionSizeInCL, 0, accessLabels);
			pc++;

			// Start---Innermost loop
			inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_numFeaturesInCL*m_partitionSize, 0, accessSamples);
			inst[pc].MakeNonBlocking();
			uint32_t pcRestSamples = pc;
			pc++;

			inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
			pc++;

			inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
			pc++;

			inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
			inst[pc].AddJump(0, m_rest-1, pcRestSamples, pc+1);
			inst[pc].IncrementIndex(0);
			pc++;
			// End---Innermost loop
		}

		// WriteBack
		inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
			0, 0, m_numFeaturesInCL,
			0, false, writebackModel);
		pc++;

		inst[pc].AddJump(2, numEpochs-1, pcLabels, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		auto outputHandle = m_fpga->allocBuffer((numEpochs*m_numFeaturesInCL+1)*64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

		RunProgram(inst, pc, output);

		// Verify
		xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = Loss(type, xHistory + e*m_alignedNumFeatures, lambda, args);
			std::cout << "loss " << e << ": " << loss << std::endl;
		}
	}

	void fSCD(
		ModelType type, 
		float* xHistory, 
		uint32_t numEpochs,
		float stepSize, 
		float lambda, 
		AdditionalArguments* args)
	{
		if (m_memory == nullptr) {
			cout << "m_memory is nullptr!" << endl;
			exit(1);
		}

		Instruction inst[Instruction::MAX_NUM_INSTRUCTIONS];

		float scaledStepSize = stepSize/m_partitionSize;
		float scaledLambda = stepSize*lambda;

		uint32_t residualOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;
		uint32_t modelOffsetInBRAM = labelOffsetInBRAM + m_partitionSizeInCL;


		AccessProperties accessResidual(4);
		accessResidual.Set(2, residualOffsetInBRAM, m_partitionSizeInCL);

		AccessProperties accessLabels(4);
		accessLabels.Set(3, labelOffsetInBRAM, m_partitionSizeInCL);

		AccessProperties accessModel(4);
		accessModel.Set(3, modelOffsetInBRAM, m_numFeaturesInCL);

		AccessProperties accessSamples(4);
		accessSamples.Set(0, 0, m_partitionSizeInCL);
		accessSamples.Set(1, 0, m_partitionSizeInCL);

		AccessProperties writebackResidual(2);
		writebackResidual.Set(0, residualOffsetInBRAM, m_partitionSizeInCL);

		AccessProperties writebackModel(2);
		writebackModel.Set(1, modelOffsetInBRAM, m_numFeaturesInCL);


		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		// Load residual
		inst[pc].Load(m_residualChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessResidual);
		pc++;

		// Load labels in partition
		inst[pc].Load(m_labelChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessLabels);
		pc++;

		// Load model
		inst[pc].Load(m_modelChunk.m_offsetInCL, m_numFeaturesInCL, 0, m_numFeaturesInCL, 0, accessModel);
		pc++;

		// Load samples
		inst[pc].Load(m_samplesChunk.m_offsetInCL, m_partitionSizeInCL, m_numSamplesInCL, m_partitionSizeInCL, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(m_partitionSizeInCL, false, true, residualOffsetInBRAM, labelOffsetInBRAM);
		pc++;

		// Start---Innermost loop
		inst[pc].Modify(modelOffsetInBRAM, type, 1, scaledStepSize, scaledLambda);
		inst[pc].MakeNonBlocking();
		uint32_t pcModify = pc;
		pc++;

		inst[pc].Update(residualOffsetInBRAM, m_partitionSizeInCL, true);
		inst[pc].IncrementIndex(0);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Load(m_samplesChunk.m_offsetInCL, m_partitionSizeInCL, m_numSamplesInCL, m_partitionSizeInCL, 0, accessSamples);
		inst[pc].MakeNonBlocking();
		pc++;

		inst[pc].Dot(m_partitionSizeInCL, true, true, residualOffsetInBRAM, labelOffsetInBRAM);
		inst[pc].AddJump(0, m_cstore->m_numFeatures-1, pcModify, pc+1);
		pc++;
		// End---Innermost loop

		inst[pc].Modify(modelOffsetInBRAM, type, 1, scaledStepSize, scaledLambda);
		pc++;

		inst[pc].Update(residualOffsetInBRAM, m_partitionSizeInCL, false);
		pc++;

		inst[pc].WriteBack(true, m_residualChunk.m_offsetInCL, m_partitionSizeInCL,
			0, m_partitionSizeInCL, 0,
			0, true, writebackResidual);
		pc++;

		inst[pc].WriteBack(true, m_modelChunk.m_offsetInCL, m_numFeaturesInCL,
			0, m_numFeaturesInCL, 0,
			1, true, writebackModel);
		inst[pc].AddJump(1, m_numPartitions-1, 0, pc+1);
		inst[pc].ResetIndex(0);
		inst[pc].IncrementIndex(1);
		pc++;

		inst[pc].AddJump(2, numEpochs-1, 0, 0xFFFFFFFF);
		inst[pc].ResetIndex(0);
		inst[pc].ResetIndex(1);
		inst[pc].IncrementIndex(2);
		pc++;
		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************

		auto outputHandle = m_fpga->allocBuffer(64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

		RunProgram(inst, pc, output);

		// Verify
		std::vector<float> avgModel(m_alignedNumFeatures);
		for (uint32_t p = 0; p < m_numPartitions; p++) {
			for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
				if (p == 0) {
					avgModel[j] = m_model[p*m_alignedNumFeatures + j];
				}
				else {
					avgModel[j] += m_model[p*m_alignedNumFeatures + j];
				}
				// cout << "p: " << p << ", avgModel[" << j <<  "]: " << avgModel[j] << endl;
			}
		}
		for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
			avgModel[j] /= m_numPartitions;
		}
		float loss = Loss(type, avgModel.data(), lambda, args);
		std::cout << "loss: " << loss << std::endl;
	}

};