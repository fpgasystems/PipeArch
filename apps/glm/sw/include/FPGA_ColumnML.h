#pragma once

#include "ColumnML.h"
#include "iFPGA.h"
#include "Instruction.h"

enum MemoryFormat {RowStore, ColumnStore};

class FPGA_ColumnML : public ColumnML {
protected:
	iFPGA* m_ifpga;

	volatile float* m_base = nullptr;
	volatile float* m_model = nullptr;
	volatile float* m_labels = nullptr;
	volatile float* m_samples = nullptr;
	volatile float* m_residual = nullptr;
	volatile uint32_t* m_accessprops = nullptr;
	volatile float** m_columns = nullptr;

	remoteaccess_t m_modelChunk;
	remoteaccess_t m_labelsChunk;
	remoteaccess_t m_samplesChunk;
	remoteaccess_t m_residualChunk;
	remoteaccess_t m_accesspropsChunk;
	remoteaccess_t** m_columnsChunks;

	MemoryFormat m_currentMemoryFormat;
	bool m_useContextSwitch;

	void WriteProgramMemory(uint32_t pcContextStore, uint32_t pcContextLoad) {
		cout << "WriteProgramMemory..." << endl;

		uint32_t pcStart = 0;
		auto output = iFPGA::CastToInt(m_outputHandle);
		output[0] = 0; // Done
		output[1] = 0; // reg 0;
		output[2] = 0; // reg 1;
		output[3] = 0; // reg 2;
		output[4] = ((pcContextLoad&0xFF) << 16) | ((pcContextStore&0xFF) << 8) | (pcStart&0xFF);
		cout << "m_numInstructions: " << m_numInstructions << endl;

		m_ifpga->Realloc(m_programMemoryHandle, m_numInstructions*Instruction::NUM_BYTES);
		auto programMemory = iFPGA::CastToInt(m_programMemoryHandle);
		assert(NULL != programMemory);
		for (uint32_t i = 0; i < m_numInstructions; i++) {
			m_inst[i].LoadInstruction(programMemory + i*Instruction::NUM_WORDS);
		}
	}

public:
	iFPGA_ptr m_inputHandle;
	iFPGA_ptr m_outputHandle;
	iFPGA_ptr m_programMemoryHandle;

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
	uint32_t m_outputSizeInCL;
	uint32_t m_numEpochs;

	Instruction m_inst[Instruction::MAX_NUM_INSTRUCTIONS];
	uint32_t m_numInstructions;

	FPGA_ColumnML(iFPGA* ifpga) {
		m_inputHandle = NULL;
		m_outputHandle = NULL;
		m_programMemoryHandle = NULL;
		m_ifpga = ifpga;
		m_useContextSwitch = true;
	}

	FPGA_ColumnML(iFPGA* ifpga, bool useContextSwitch) {
		m_inputHandle = NULL;
		m_outputHandle = NULL;
		m_programMemoryHandle = NULL;
		m_ifpga = ifpga;
		m_useContextSwitch = useContextSwitch;
	}

	~FPGA_ColumnML() {
		m_ifpga->Free(m_inputHandle);
		m_ifpga->Free(m_outputHandle);
		m_ifpga->Free(m_programMemoryHandle);
	}

	void ResetContext() {
		auto output = iFPGA::CastToInt(m_outputHandle);
		output[0] = 0; // Done
		output[1] = 0; // reg 0;
		output[2] = 0; // reg 1;
		output[3] = 0; // reg 2;
	}

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize) {
		return CreateMemoryLayout(format, partitionSize, 1);
	}

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize, uint32_t numEpochs) {
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
		m_numEpochs = numEpochs;

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

		if (format == RowStore) {
			// Model
			m_modelChunk.m_offsetInCL = countCL;
			countCL += m_numFeaturesInCL;
			m_modelChunk.m_lengthInCL = countCL - m_modelChunk.m_offsetInCL;

			// Labels
			m_labelsChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_labelsChunk.m_lengthInCL = countCL - m_labelsChunk.m_offsetInCL;

			// Samples
			m_samplesChunk.m_offsetInCL = countCL;
			countCL += m_cstore->m_numSamples*m_numFeaturesInCL;
			m_samplesChunk.m_lengthInCL = countCL - m_samplesChunk.m_offsetInCL;

			m_ifpga->Realloc(m_inputHandle, countCL*64);
			m_base = iFPGA::CastToFloat(m_inputHandle);
			memset((void*)m_base, 0, 16*countCL*sizeof(float));

			m_model = m_base + m_modelChunk.m_offsetInCL*16;
			m_labels = m_base + m_labelsChunk.m_offsetInCL*16;
			m_samples = m_base + m_samplesChunk.m_offsetInCL*16;

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
		else if (format == ColumnStore) {
			// Residual
			m_residualChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_residualChunk.m_lengthInCL = countCL - m_residualChunk.m_offsetInCL;
#ifdef DEBUG_COPY
			cout << "m_residualChunk.m_offsetInCL: " << m_residualChunk.m_offsetInCL << endl;
			cout << "m_residualChunk.m_lengthInCL: " << m_residualChunk.m_lengthInCL << endl;
#endif
			// Labels
			m_labelsChunk.m_offsetInCL = countCL;
			countCL += m_numSamplesInCL;
			m_labelsChunk.m_lengthInCL = countCL - m_labelsChunk.m_offsetInCL;
#ifdef DEBUG_COPY
			cout << "m_labelsChunk.m_offsetInCL: " << m_labelsChunk.m_offsetInCL << endl;
			cout << "m_labelsChunk.m_lengthInCL: " << m_labelsChunk.m_lengthInCL << endl;
#endif
			// Model
			m_modelChunk.m_offsetInCL = countCL;
			countCL += m_numEpochs*m_numPartitions*m_numFeaturesInCL;
			m_modelChunk.m_lengthInCL = countCL - m_modelChunk.m_offsetInCL;
#ifdef DEBUG_COPY
			cout << "m_modelChunk.m_offsetInCL: " << m_modelChunk.m_offsetInCL << endl;
			cout << "m_modelChunk.m_lengthInCL: " << m_modelChunk.m_lengthInCL << endl;
#endif
			// Offsets
			m_accesspropsChunk.m_offsetInCL = countCL;
			countCL += m_numPartitions*m_numFeaturesInCL*2; // *2, because offset and length
			m_accesspropsChunk.m_lengthInCL = countCL - m_accesspropsChunk.m_offsetInCL;
#ifdef DEBUG_COPY
			cout << "m_accesspropsChunk.m_offsetInCL: " << m_accesspropsChunk.m_offsetInCL << endl;
			cout << "m_accesspropsChunk.m_lengthInCL: " << m_accesspropsChunk.m_lengthInCL << endl;
#endif
			// Columns
			m_columnsChunks = new remoteaccess_t*[m_cstore->m_numFeatures];
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				m_columnsChunks[j] = new remoteaccess_t[m_numPartitions];
				for (uint32_t p = 0; p < m_numPartitions; p++) {
					m_columnsChunks[j][p].m_offsetInCL = countCL;
					countCL += m_partitionSizeInCL;
					m_columnsChunks[j][p].m_lengthInCL = countCL - m_columnsChunks[j][p].m_offsetInCL;
#ifdef DEBUG_COPY
					cout << "m_columnsChunks[" << j << "," << p << "].m_offsetInCL: " << m_columnsChunks[j][p].m_offsetInCL << endl;
					cout << "m_columnsChunks[" << j << "," << p << "].m_lengthInCL: " << m_columnsChunks[j][p].m_lengthInCL << endl;
#endif
				}
			}

			m_ifpga->Realloc(m_inputHandle, countCL*64);
			m_base = iFPGA::CastToFloat(m_inputHandle);
			memset((void*)m_base, 0, 16*countCL*sizeof(float));

			m_residual = m_base + m_residualChunk.m_offsetInCL*16;
			m_labels = m_base + m_labelsChunk.m_offsetInCL*16;
			m_model = m_base + m_modelChunk.m_offsetInCL*16;
			m_accessprops = (uint32_t*)(m_base + m_accesspropsChunk.m_offsetInCL*16);
			m_columns = new volatile float*[m_cstore->m_numFeatures];
			for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
				m_columns[j] = m_base + m_columnsChunks[j][0].m_offsetInCL*16;
			}

			for (uint32_t i = 0; i < m_cstore->m_numSamples; i++) {
				m_residual[i] = 0;
				m_labels[i] = m_cstore->m_labels[i];
			}
			for (uint32_t e = 0; e < m_numEpochs; e++) {
				for (uint32_t p = 0; p < m_numPartitions; p++) {
					for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
						m_model[e*m_numPartitions*m_alignedNumFeatures + p*m_alignedNumFeatures + j] = 0;
					}
				}
			}
			for (uint32_t p = 0; p < m_numPartitions; p++) {
				for (uint32_t j = 0; j < m_cstore->m_numFeatures; j++) {
					m_accessprops[p*m_alignedNumFeatures*2 + 2*j] = m_columnsChunks[j][p].m_offsetInCL;
					m_accessprops[p*m_alignedNumFeatures*2 + 2*j+1] = m_columnsChunks[j][p].m_lengthInCL;
					for (uint32_t i = 0; i < m_partitionSize; i++) {
						m_columns[j][p*m_alignedPartitionSize + i] = m_cstore->m_samples[j][p*m_alignedPartitionSize + i];
					}
				}
			}
		}

		return countCL;
	}

	bool fSGD(
		ModelType type,
		uint32_t numEpochs,
		float stepSize,
		float lambda);

	bool fSGD_minibatch(
		ModelType type,
		uint32_t numEpochs,
		uint32_t minibatchSize, 
		float stepSize,
		float lambda);

	bool fSCD(
		ModelType type, 
		uint32_t numEpochs,
		float stepSize, 
		float lambda);

	bool fSGD_blocking(
		ModelType type,
		uint32_t numEpochs,
		float stepSize,
		float lambda);

	vector<float> GetModelSCD(uint32_t epoch) {
		vector<float> avgModel(m_alignedNumFeatures, 0);
		for (uint32_t e = 0; e < epoch+1; e++) {
			for (uint32_t p = 0; p < m_numPartitions; p++) {
				for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
					avgModel[j] += m_model[e*m_numPartitions*m_alignedNumFeatures + p*m_alignedNumFeatures + j];
				}
			}
		}
		for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
			avgModel[j] /= m_numPartitions;
		}
		return avgModel;
	}

	void ReadBandwidth(uint32_t numIterations);
	void Correctness();
};