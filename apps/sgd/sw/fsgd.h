#include "ColumnML.h"
#include "iFPGA.h"

class fsgd : public iFPGA, public ColumnML {

public:
	fsgd(const char* accel_uuid) : iFPGA(accel_uuid) {}

	volatile float* m_memory = nullptr;
	volatile float* m_model = nullptr;
	volatile float* m_labels = nullptr;
	volatile float* m_samples = nullptr;

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

	uint32_t CreateMemoryLayout(uint32_t partitionSize) {

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


		if (m_handle != NULL) {
			m_handle->release();
			m_handle = NULL;
		}
		m_handle = m_fpga->allocBuffer(countCL*64);
		m_memory = reinterpret_cast<volatile float*>(m_handle->c_type());
		assert(NULL != m_memory);

		memset((void*)m_memory, 0, 16*countCL*sizeof(float));

		m_model = m_memory + m_modelChunk.m_offsetInCL*16;
		m_labels = m_memory + m_labelChunk.m_offsetInCL*16;
		m_samples = m_memory + m_samplesChunk.m_offsetInCL*16;

		for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
			m_model[j] = 0.1;
		}
		for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
			m_labels[i] = m_cstore->m_labels[i];
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				m_samples[i*m_alignedNumFeatures + j] = m_cstore->m_samples[j][i];
			}
		}

		return countCL;
	}

	void CopyDataToFPGAMemory(uint32_t partitionSize) {
		uint32_t countCL = CreateMemoryLayout(partitionSize);
		cout << "CopyDataToFPGAMemory END" << endl;
	}

	void fSGD(
		ModelType type,
		float* xHistory,
		uint32_t numEpochs,
		float stepSize,
		float lambda)
	{
		if (m_memory == nullptr) {
			return;
		}

		const uint32_t numInstructions = 8;
		Instruction inst[numInstructions];

		uint32_t modelOffsetInBRAM = 0;
		uint32_t labelOffsetInBRAM = 0;

		access_t accessInternal[3];

		// Load model
		accessInternal[0].m_offsetInCL = 0;
		accessInternal[0].m_lengthInCL = 0;
		accessInternal[1].m_offsetInCL = modelOffsetInBRAM;
		accessInternal[1].m_lengthInCL = m_modelChunk.m_lengthInCL;
		accessInternal[2].m_offsetInCL = 0;
		accessInternal[2].m_lengthInCL = 0;
		inst[0].Load(
			m_modelChunk.m_offsetInCL,
			m_modelChunk.m_lengthInCL,
			0,
			0,
			0,
			accessInternal,
			3);
		inst[0].ResetIndex(0);
		inst[0].ResetIndex(1);
		inst[0].ResetIndex(2);

		// Load labels
		accessInternal[0].m_offsetInCL = 0;
		accessInternal[0].m_lengthInCL = 0;
		accessInternal[1].m_offsetInCL = 0;
		accessInternal[1].m_lengthInCL = 0;
		accessInternal[2].m_offsetInCL = labelOffsetInBRAM;
		accessInternal[2].m_lengthInCL = m_numSamplesInCL;
		inst[1].Load(
			m_labelChunk.m_offsetInCL,
			m_numSamplesInCL,
			0,
			0,
			0,
			accessInternal,
			3);

		// Prefetch all samples
		inst[2].Prefetch(
			m_samplesChunk.m_offsetInCL,
			m_cstore->m_numSamples*m_numFeaturesInCL,
			0,
			0,
			0);

		// Innermost loop

		// Load samples
		accessInternal[0].m_offsetInCL = 0;
		accessInternal[0].m_lengthInCL = m_numFeaturesInCL;
		accessInternal[1].m_offsetInCL = 0;
		accessInternal[1].m_lengthInCL = 0;
		accessInternal[2].m_offsetInCL = 0;
		accessInternal[2].m_lengthInCL = 0;
		inst[3].Load(
			m_samplesChunk.m_offsetInCL,
			m_numFeaturesInCL,
			m_numFeaturesInCL, // Offset by index 0
			0,
			0,
			accessInternal,
			3);
		inst[3].MakeNonBlocking();

		// Dot
		access_t accessPropertiesOut_dot;
		accessPropertiesOut_dot.m_offsetInCL = 0;
		accessPropertiesOut_dot.m_lengthInCL = 1;
		inst[4].Dot_dot(
			accessPropertiesOut_dot,
			m_numFeaturesInCL);

		// Subtract
		access_t accessPropertiesIn_labelsMem;
		accessPropertiesIn_labelsMem.m_offsetInCL = 0;
		accessPropertiesIn_labelsMem.m_lengthInCL = 1;
		inst[5].ScalarSubtract_error(
			accessPropertiesIn_labelsMem,
			1);

		// WriteBack
		access_t accessOut[1];
		accessOut[0].m_offsetInCL = 0;
		accessOut[0].m_lengthInCL = 1;
		inst[6].WriteBack(
			1,
			1,
			1, // Offset by index 0
			0,
			0,
			0,
			accessOut,
			1);
		inst[6].IncrementIndex(0);

		inst[7].Jump(0, m_cstore->m_numSamples, 3, 0xFFFFFFFF);


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

		m_csrs->writeCSR(0, intptr_t(m_memory));
		m_csrs->writeCSR(1, intptr_t(output));
		m_csrs->writeCSR(2, intptr_t(programMemory));
		m_csrs->writeCSR(3, (uint64_t)instructions.size());

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (m_fpga->hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 2500000;

		output[0] = 0;
		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};


		// Verify
		for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
			float dot = 0.0;
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				dot += m_samples[i*m_alignedNumFeatures + j]*m_model[j];
			}
			float error = dot - m_labels[i];
			if (error != output[16*(i+1)]) {
				cout << "Missmatch: error = " << error << ", output[" << 16*(i+1) << "]: " << output[16*(i+1)] << endl;
			}
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