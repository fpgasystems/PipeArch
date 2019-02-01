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
		m_numPartitions = m_numSamplesInCL/m_partitionSizeInCL + (m_numSamplesInCL%m_partitionSizeInCL > 0);

		std::cout << "m_numSamplesInCL: " << m_numSamplesInCL << std::endl;
		std::cout << "m_numFeaturesInCL: " << m_numFeaturesInCL << std::endl;
		std::cout << "m_partitionSize: " << m_partitionSize << std::endl;
		std::cout << "m_partitionSizeInCL: " << m_partitionSizeInCL << std::endl;
		std::cout << "m_alignedNumSamples: " << m_alignedNumSamples << std::endl;
		std::cout << "m_alignedNumFeatures: " << m_alignedNumFeatures << std::endl;
		std::cout << "m_alignedPartitionSize: " << m_alignedPartitionSize << std::endl;
		std::cout << "m_numPartitions: " << m_numPartitions << std::endl;

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

	void fSGD(
		ModelType type,
		float* xHistory,
		uint32_t numEpochs,
		float stepSize,
		float lambda,
		AdditionalArguments* args)
	{
		if (m_memory == nullptr) {
			return;
		}

		const uint32_t numInstructions = 15;
		Instruction inst[numInstructions];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		access_t accessRead[4];

		// Load model
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = 0;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = 0;
		accessRead[2].m_offsetInCL = modelOffsetInBRAM;
		accessRead[2].m_lengthInCL = m_modelChunk.m_lengthInCL;
		accessRead[3].m_offsetInCL = 0;
		accessRead[3].m_lengthInCL = 0;
		inst[0].Load(
			m_modelChunk.m_offsetInCL,
			m_modelChunk.m_lengthInCL,
			0,
			0,
			0,
			accessRead,
			4);
		inst[0].ResetIndex(0);
		inst[0].ResetIndex(1);
		inst[0].ResetIndex(2);

		// Load labels in partition
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = 0;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = 0;
		accessRead[2].m_offsetInCL = 0;
		accessRead[2].m_lengthInCL = 0;
		accessRead[3].m_offsetInCL = labelOffsetInBRAM;
		accessRead[3].m_lengthInCL = m_partitionSize;
		inst[1].Load(
			m_labelChunk.m_offsetInCL,
			m_partitionSize,
			0,
			m_partitionSize,
			0,
			accessRead,
			4);

		// Prefetch all samples
		inst[2].Prefetch(
			m_samplesChunk.m_offsetInCL,
			m_cstore->m_numSamples*m_numFeaturesInCL,
			0,
			0,
			0);

		// Load samples
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = m_numFeaturesInCL;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = m_numFeaturesInCL;
		accessRead[2].m_offsetInCL = 0;
		accessRead[2].m_lengthInCL = 0;
		accessRead[3].m_offsetInCL = 0;
		accessRead[3].m_lengthInCL = 0;
		inst[3].Load(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL, // Offset by index 0
			0,
			0,
			accessRead,
			4);
		inst[3].MakeNonBlocking();

		inst[4].Dot(
			m_numFeaturesInCL,
			false,
			false,
			modelOffsetInBRAM,
			0xFFFF);

		// Innermost loop
		inst[5].Modify(
			labelOffsetInBRAM,
			type,
			0,
			stepSize,
			lambda);
		inst[5].IncrementIndex(0);
		inst[5].MakeNonBlocking();

		inst[6].Update(
			modelOffsetInBRAM,
			m_numFeaturesInCL,
			true);
		inst[6].MakeNonBlocking();

		// Load samples
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = m_numFeaturesInCL;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = m_numFeaturesInCL;
		accessRead[2].m_offsetInCL = 0;
		accessRead[2].m_lengthInCL = 0;
		accessRead[3].m_offsetInCL = 0;
		accessRead[3].m_lengthInCL = 0;
		inst[7].Load(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL, // Offset by index 0
			0,
			0,
			accessRead,
			4);
		inst[7].MakeNonBlocking();

		inst[8].Dot(
			m_numFeaturesInCL,
			true,
			false,
			0,
			0xFFFF);

		// End of samples
		inst[9].Jump(0, m_partitionSize-1, 5, 10);

		inst[10].Modify(
			labelOffsetInBRAM,
			type,
			0,
			stepSize,
			lambda);

		inst[11].Update(
			modelOffsetInBRAM,
			m_numFeaturesInCL,
			false);
		inst[11].ResetIndex(0);
		inst[11].IncrementIndex(1);

		inst[12].Jump(1, m_numPartitions, 1, 13);

		// WriteBack
		access_t accessOut[1];
		accessOut[0].m_offsetInCL = 0;
		accessOut[0].m_lengthInCL = m_numFeaturesInCL;
		inst[13].WriteBack(
			1,
			m_numFeaturesInCL,
			0,
			0,
			m_numFeaturesInCL,
			0,
			accessOut,
			1);
		inst[13].IncrementIndex(2);

		inst[14].Jump(2, numEpochs, 1, 0xFFFFFFFF);
		inst[14].ResetIndex(0);
		inst[14].ResetIndex(1);

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

		auto outputHandle = m_fpga->allocBuffer(numEpochs*m_numFeaturesInCL*64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

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

		// Verify
		xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = Loss(type, xHistory + e*m_alignedNumFeatures, lambda, args);
			std::cout << "loss " << e << ": " << loss << std::endl;
		}

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

	void fSGD_blocking(
		ModelType type,
		float* xHistory,
		uint32_t numEpochs,
		float stepSize,
		float lambda,
		AdditionalArguments* args)
	{
		if (m_memory == nullptr) {
			return;
		}

		const uint32_t numInstructions = 15;
		Instruction inst[numInstructions];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		access_t accessRead[4];

		// Load model
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = 0;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = 0;
		accessRead[2].m_offsetInCL = modelOffsetInBRAM;
		accessRead[2].m_lengthInCL = m_modelChunk.m_lengthInCL;
		accessRead[3].m_offsetInCL = 0;
		accessRead[3].m_lengthInCL = 0;
		inst[0].Load(
			m_modelChunk.m_offsetInCL,
			m_modelChunk.m_lengthInCL,
			0,
			0,
			0,
			accessRead,
			4);
		inst[0].ResetIndex(0);
		inst[0].ResetIndex(1);
		inst[0].ResetIndex(2);

		// Load labels in partition
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = 0;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = 0;
		accessRead[2].m_offsetInCL = 0;
		accessRead[2].m_lengthInCL = 0;
		accessRead[3].m_offsetInCL = labelOffsetInBRAM;
		accessRead[3].m_lengthInCL = m_partitionSizeInCL;
		inst[1].Load(
			m_labelChunk.m_offsetInCL,
			m_partitionSizeInCL,
			0,
			m_partitionSizeInCL, // Offset by index 1
			0,
			accessRead,
			4);

		// Innermost loop

		// Load samples
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = m_numFeaturesInCL;
		accessRead[1].m_offsetInCL = 0;
		accessRead[1].m_lengthInCL = m_numFeaturesInCL;
		accessRead[2].m_offsetInCL = 0;
		accessRead[2].m_lengthInCL = 0;
		accessRead[3].m_offsetInCL = 0;
		accessRead[3].m_lengthInCL = 0;
		inst[2].Load(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL, // Offset by index 0
			m_numFeaturesInCL*m_partitionSize, // Offset by index 1
			0,
			accessRead,
			4);
		inst[2].MakeNonBlocking();

		inst[3].Dot(
			m_numFeaturesInCL,
			false,
			false,
			modelOffsetInBRAM,
			0xFFFF);

		inst[4].Modify(
			labelOffsetInBRAM,
			type,
			0,
			stepSize,
			lambda);

		inst[5].Update(
			modelOffsetInBRAM,
			m_numFeaturesInCL,
			false);

		// End of samples
		inst[6].Jump(0, m_partitionSize-1, 2, 7);
		inst[6].IncrementIndex(0);

		inst[7].Jump(1, m_numPartitions-1, 1, 8);
		inst[7].ResetIndex(0);
		inst[7].IncrementIndex(1);

		// WriteBack
		access_t accessOut[1];
		accessOut[0].m_offsetInCL = 0;
		accessOut[0].m_lengthInCL = m_numFeaturesInCL;
		inst[8].WriteBack(
			1,
			m_numFeaturesInCL,
			0,
			0,
			m_numFeaturesInCL,
			0,
			accessOut,
			1);

		inst[9].Jump(2, numEpochs-1, 1, 0xFFFFFFFF);
		inst[9].IncrementIndex(2);
		inst[9].ResetIndex(0);
		inst[9].ResetIndex(1);

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

		auto outputHandle = m_fpga->allocBuffer(numEpochs*m_numFeaturesInCL*64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

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

		// Verify
		xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = Loss(type, xHistory + e*m_alignedNumFeatures, lambda, args);
			std::cout << "loss " << e << ": " << loss << std::endl;
		}

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
};