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

#include "LRMF.h"
#include "FPGA_Program.h"
#include "iFPGA.h"
#include "Instruction.h"

class FPGA_LRMF : public LRMF, public FPGA_Program {
private:
	vector<remoteaccess_t> m_MindexesChunks;
	vector<remoteaccess_t> m_UindexesChunks;
	vector<remoteaccess_t> m_ValuesChunks;

	volatile float* m_Mptr = nullptr;
	volatile float* m_Uptr = nullptr;
	volatile remoteaccess_t* m_accessMindexes = nullptr;
	volatile remoteaccess_t* m_accessUindexes = nullptr;
	volatile remoteaccess_t* m_accessValues = nullptr;
	vector<volatile uint32_t*> m_MindexesPtr;
	vector<volatile uint32_t*> m_UindexesPtr;
	vector<volatile float*> m_ValuesPtr;
	volatile uint32_t* m_minibatchSizes;

public:
	remoteaccess_t m_Mchunk;
	remoteaccess_t m_Uchunk;
	remoteaccess_t m_accessMindexesChunk;
	remoteaccess_t m_accessUindexesChunk;
	remoteaccess_t m_accessValuesChunk;
	remoteaccess_t m_minibatchSizesChunk;

	uint32_t m_numFeaturesInCL;
	uint32_t m_alignedNumFeatures;
	uint32_t m_maxBatchSize;
	uint32_t m_numAccessIndexesPerTileInCL;
	uint32_t m_numLocalIndexesPerTileInCL;

	FPGA_LRMF(iFPGA* ifpga, uint32_t numFeatures, bool useContextSwitch) : LRMF(numFeatures), FPGA_Program(ifpga, useContextSwitch) {}
	FPGA_LRMF(iFPGA* ifpga, uint32_t numFeatures) : LRMF(numFeatures), FPGA_Program(ifpga, false) {}
	FPGA_LRMF(iFPGA* ifpga) : LRMF(0), FPGA_Program(ifpga, false) {}

	inline uint32_t ConvertNumWordToNumCL(uint32_t numWords, uint32_t wordSizeInBytes) {
		uint32_t numWordsInCL = 64/wordSizeInBytes;
		return numWords/numWordsInCL + (numWords%numWordsInCL > 0);
	}

	void CopyModel() {
		for (uint32_t m = 0; m < m_Mdim; m++) {
			for (uint32_t j = 0; j < LRMF::LRMF::m_numFeatures; j++) {
				m_M[m*LRMF::LRMF::m_numFeatures + j] = m_Mptr[m*m_alignedNumFeatures + j];
			}
		}
		for (uint32_t u = 0; u < m_Udim; u++) {
			for (uint32_t j = 0; j < LRMF::LRMF::m_numFeatures; j++) {
				m_U[u*LRMF::LRMF::m_numFeatures + j] = m_Uptr[u*m_alignedNumFeatures + j];
			}
		}
	}

	void UseCreatedMemoryLayout(FPGA_LRMF* baseObj);
	uint32_t CreateMemoryLayout();

	bool fOptimizeRoundDoubleBuffer(
		uint32_t MtileToStart,
		uint32_t numTilesM,
		uint32_t UtileToStart,
		uint32_t numTilesU,
		float stepSize,
		float lambda,
		bool asyncUpdate,
		bool staleRead,
		uint32_t numEpochs);

	bool fOptimizeRound(
		uint32_t MtileToStart,
		uint32_t numTilesM,
		uint32_t UtileToStart,
		uint32_t numTilesU,
		float stepSize,
		float lambda,
		bool asyncUpdate,
		bool staleRead,
		uint32_t numEpochs);
};