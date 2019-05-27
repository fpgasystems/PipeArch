// Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//*************************************************************************

#pragma once

#include "ColumnML.h"
#include "iFPGA.h"
#include "Instruction.h"
#include "FPGA_Program.h"

enum MemoryFormat {RowStore, ColumnStore};

class FPGA_ColumnML : public ColumnML, public FPGA_Program {
protected:

	volatile float* m_model = nullptr;
	volatile float* m_labels = nullptr;
	volatile float* m_samples = nullptr;
	volatile float* m_residual = nullptr;
	volatile uint32_t* m_accessprops = nullptr;
	vector<volatile float*> m_columns;

public:
	remoteaccess_t m_modelChunk;
	remoteaccess_t m_labelsChunk;
	remoteaccess_t m_samplesChunk;
	remoteaccess_t m_residualChunk;
	remoteaccess_t m_accesspropsChunk;
	vector<remoteaccess_t> m_columnsChunks;

	uint32_t m_numSamples;
	uint32_t m_numFeatures;
	uint32_t m_numSamplesInCL;
	uint32_t m_numFeaturesInCL;
	uint32_t m_partitionSize;
	uint32_t m_partitionSizeInCL;
	uint32_t m_alignedNumSamples;
	uint32_t m_alignedNumFeatures;
	uint32_t m_alignedPartitionSize;
	uint32_t m_numPartitions;
	uint32_t m_rest;
	uint32_t m_restInCL;
	uint32_t m_numEpochs;
	MemoryFormat m_currentMemoryFormat;

	uint32_t GetBank() {
		return m_ifpga->GetBank();
	}

	FPGA_ColumnML(iFPGA* ifpga) : FPGA_Program (ifpga, true) {}

	FPGA_ColumnML(iFPGA* ifpga, bool useContextSwitch) : FPGA_Program(ifpga, useContextSwitch) {}

	void UseCreatedMemoryLayout(FPGA_ColumnML* baseObj);

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize) {
		return CreateMemoryLayout(format, partitionSize, 1, false);
	}

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize, bool useOnehotLabels) {
		return CreateMemoryLayout(format, partitionSize, 1, useOnehotLabels);
	}

	uint32_t CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize, uint32_t numEpochs, bool useOnehotLabels);

	bool fSGD(
		ModelType type,
		uint32_t numEpochs,
		float stepSize,
		float lambda,
		uint32_t whichClass);

	bool fSGD_minibatch(
		ModelType type,
		uint32_t numEpochs,
		uint32_t minibatchSize, 
		float stepSize,
		float lambda,
		uint32_t whichClass);

	bool fSCD(
		uint32_t partitionToStart,
		uint32_t numPartitions,
		ModelType type, 
		uint32_t numEpochs,
		float stepSize, 
		float lambda);

	bool fSGD_blocking(
		ModelType type,
		uint32_t numEpochs,
		float stepSize,
		float lambda);

	bool ReadBandwidth(uint32_t numLinesToRead, uint32_t numLinesToWrite, uint32_t numIterations);

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

	vector<float> GetModelSCD(uint32_t epoch, uint32_t partitionToStart, uint32_t numPartitions) {
		m_model = m_base + m_modelChunk.m_offsetInCL*16; // Repeat this here, because it might be using other instances memory

		vector<float> avgModel(m_alignedNumFeatures, 0);
		for (uint32_t e = 0; e < epoch+1; e++) {
			for (uint32_t p = partitionToStart; p < partitionToStart+numPartitions; p++) {
				for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
					avgModel[j] += m_model[e*m_numPartitions*m_alignedNumFeatures + p*m_alignedNumFeatures + j];
				}
			}
		}
		for (uint32_t j = 0; j < m_alignedNumFeatures; j++) {
			avgModel[j] /= numPartitions;
		}
		return avgModel;
	}
};