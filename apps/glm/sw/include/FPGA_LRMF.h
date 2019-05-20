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
#include "FPGA_ColumnML.h"
#include "iFPGA.h"
#include "Instruction.h"

class FPGA_LRMF : public LRMF, public FPGA_ColumnML {
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

	FPGA_LRMF(iFPGA* ifpga, uint32_t numFeatures) : LRMF(numFeatures), FPGA_ColumnML(ifpga) {}

	FPGA_LRMF(iFPGA* ifpga) : LRMF(0), FPGA_ColumnML(ifpga) {}

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

	void UseCreatedMemoryLayout(FPGA_LRMF* baseObj) {
		if (m_inputHandle != NULL) {
			cout << "m_inputHandle of FPGA_LRMF object created as clone is not NULL" << endl;
			return;
		}

		m_base = baseObj->GetBase();

		m_tileSize = baseObj->GetTileSize();

		m_inputHandle = baseObj->m_inputHandle;

		m_Mchunk = baseObj->m_Mchunk;
		m_Uchunk = baseObj->m_Uchunk;
		m_accessMindexesChunk = baseObj->m_accessMindexesChunk;
		m_accessUindexesChunk = baseObj->m_accessUindexesChunk;
		m_accessValuesChunk = baseObj->m_accessValuesChunk;
		m_minibatchSizesChunk = baseObj->m_minibatchSizesChunk;

		m_numFeaturesInCL = baseObj->m_numFeaturesInCL;
		m_alignedNumFeatures = baseObj->m_alignedNumFeatures;
		m_maxBatchSize = baseObj->m_maxBatchSize;
		m_numAccessIndexesPerTileInCL = baseObj->m_numAccessIndexesPerTileInCL;
		m_numLocalIndexesPerTileInCL = baseObj->m_numLocalIndexesPerTileInCL;
	}

	uint32_t CreateMemoryLayout() {
		if (LRMF::LRMF::m_numFeatures == 0) {
			cout << "LRMF::m_numFeatures of FPGA_LRMF object created as main is equal to 0" << endl;
			return 0;
		}
		if (m_LBTiled.size() == 0) {
			cout << "m_LBTiled is empty" << endl;
			return 0;
		}

		m_numFeaturesInCL = ConvertNumWordToNumCL(LRMF::LRMF::m_numFeatures, sizeof(float));
		m_alignedNumFeatures = m_numFeaturesInCL*16;
		m_numAccessIndexesPerTileInCL = ConvertNumWordToNumCL(m_numTilesU, sizeof(remoteaccess_t));
		m_numLocalIndexesPerTileInCL = ConvertNumWordToNumCL(m_numTilesU, sizeof(uint32_t));

		cout << "m_tileSize: " << m_tileSize << endl;
		cout << "m_numTilesM: " << m_numTilesM << endl;
		cout << "m_numTilesU: " << m_numTilesU << endl;
		cout << "m_numAccessIndexesPerTileInCL: " << m_numAccessIndexesPerTileInCL << endl;
		cout << "m_numLocalIndexesPerTileInCL: " << m_numLocalIndexesPerTileInCL << endl;

		uint32_t countCL = 0;

		m_Mchunk.m_offsetInCL = countCL;
		countCL += m_numFeaturesInCL*m_numTilesM*m_tileSize;
		m_Mchunk.m_lengthInCL = countCL - m_Mchunk.m_offsetInCL;

		m_Uchunk.m_offsetInCL = countCL;
		countCL += m_numFeaturesInCL*m_numTilesU*m_tileSize;
		m_Uchunk.m_lengthInCL = countCL - m_Uchunk.m_offsetInCL;

		m_accessMindexesChunk.m_offsetInCL = countCL;
		countCL += m_numTilesM*m_numAccessIndexesPerTileInCL;
		m_accessMindexesChunk.m_lengthInCL = countCL - m_accessMindexesChunk.m_offsetInCL;

		m_accessUindexesChunk.m_offsetInCL = countCL;
		countCL += m_numTilesM*m_numAccessIndexesPerTileInCL;
		m_accessUindexesChunk.m_lengthInCL = countCL - m_accessUindexesChunk.m_offsetInCL;

		m_accessValuesChunk.m_offsetInCL = countCL;
		countCL += m_numTilesM*m_numAccessIndexesPerTileInCL;
		m_accessValuesChunk.m_lengthInCL = countCL - m_accessValuesChunk.m_offsetInCL;

		m_maxBatchSize = 0;
		m_MindexesChunks.resize(m_numTilesM*m_numTilesU);
		for (uint32_t t = 0; t < m_numTilesM*m_numTilesU; t++) {
			m_MindexesChunks[t].m_offsetInCL = countCL;
			countCL += ConvertNumWordToNumCL( m_LBTiled[t].size(), sizeof(uint32_t) );
			m_MindexesChunks[t].m_lengthInCL = countCL - m_MindexesChunks[t].m_offsetInCL;
			if (m_LBTiled[t].size() > m_maxBatchSize) {
				m_maxBatchSize = m_LBTiled[t].size();
			}
		}

		m_UindexesChunks.resize(m_numTilesM*m_numTilesU);
		for (uint32_t t = 0; t < m_numTilesM*m_numTilesU; t++) {
			m_UindexesChunks[t].m_offsetInCL = countCL;
			countCL += ConvertNumWordToNumCL( m_LBTiled[t].size(), sizeof(uint32_t) );
			m_UindexesChunks[t].m_lengthInCL = countCL - m_UindexesChunks[t].m_offsetInCL;
		}

		m_ValuesChunks.resize(m_numTilesM*m_numTilesU);
		for (uint32_t t = 0; t < m_numTilesM*m_numTilesU; t++) {
			m_ValuesChunks[t].m_offsetInCL = countCL;
			countCL += ConvertNumWordToNumCL( m_LBTiled[t].size(), sizeof(uint32_t) );
			m_ValuesChunks[t].m_lengthInCL = countCL - m_ValuesChunks[t].m_offsetInCL;
		}

		m_minibatchSizesChunk.m_offsetInCL = countCL;
		countCL += m_numTilesM*m_numLocalIndexesPerTileInCL;
		m_minibatchSizesChunk.m_lengthInCL = countCL - m_minibatchSizesChunk.m_offsetInCL;

		m_ifpga->Realloc(m_inputHandle, countCL*64);
		m_base = iFPGA::CastToFloat(m_inputHandle);
		memset((void*)m_base, 0, 16*countCL*sizeof(float));

		m_Mptr = m_base + m_Mchunk.m_offsetInCL*16;
		m_Uptr = m_base + m_Uchunk.m_offsetInCL*16;
		m_accessMindexes = (remoteaccess_t*)m_base + m_accessMindexesChunk.m_offsetInCL*8;
		m_accessUindexes = (remoteaccess_t*)m_base + m_accessUindexesChunk.m_offsetInCL*8;
		m_accessValues = (remoteaccess_t*)m_base + m_accessValuesChunk.m_offsetInCL*8;
		m_MindexesPtr.resize(m_numTilesM*m_numTilesU);
		m_UindexesPtr.resize(m_numTilesM*m_numTilesU);
		m_ValuesPtr.resize(m_numTilesM*m_numTilesU);
		for (uint32_t t = 0; t < m_numTilesM*m_numTilesU; t++) {
			m_MindexesPtr[t] = ((uint32_t*)m_base) + m_MindexesChunks[t].m_offsetInCL*16;
			m_UindexesPtr[t] = ((uint32_t*)m_base) + m_UindexesChunks[t].m_offsetInCL*16;
			m_ValuesPtr[t] = ((float*)m_base) + m_ValuesChunks[t].m_offsetInCL*16;
		}

		for (uint32_t m = 0; m < m_Mdim; m++) {
			for (uint32_t j = 0; j < LRMF::m_numFeatures; j++) {
				m_Mptr[m*m_alignedNumFeatures + j] = m_M[m*LRMF::m_numFeatures + j];
			}
		}
		for (uint32_t u = 0; u < m_Udim; u++) {
			for (uint32_t j = 0; j < LRMF::m_numFeatures; j++) {
				m_Uptr[u*m_alignedNumFeatures + j] = m_U[u*LRMF::m_numFeatures + j];
			}
		}
		m_minibatchSizes = ((uint32_t*)m_base) + m_minibatchSizesChunk.m_offsetInCL*16;

		for (uint32_t tm = 0; tm < m_numTilesM; tm++) {
			for (uint32_t tu = 0; tu < m_numTilesU; tu++) {

				m_accessMindexes[tm*m_numAccessIndexesPerTileInCL*8+tu].m_offsetInCL = m_MindexesChunks[tm*m_numTilesU+tu].m_offsetInCL;
				m_accessMindexes[tm*m_numAccessIndexesPerTileInCL*8+tu].m_lengthInCL = m_MindexesChunks[tm*m_numTilesU+tu].m_lengthInCL;

				m_accessUindexes[tm*m_numAccessIndexesPerTileInCL*8+tu].m_offsetInCL = m_UindexesChunks[tm*m_numTilesU+tu].m_offsetInCL;
				m_accessUindexes[tm*m_numAccessIndexesPerTileInCL*8+tu].m_lengthInCL = m_UindexesChunks[tm*m_numTilesU+tu].m_lengthInCL;

				m_accessValues[tm*m_numAccessIndexesPerTileInCL*8+tu].m_offsetInCL = m_ValuesChunks[tm*m_numTilesU+tu].m_offsetInCL;
				m_accessValues[tm*m_numAccessIndexesPerTileInCL*8+tu].m_lengthInCL = m_ValuesChunks[tm*m_numTilesU+tu].m_lengthInCL;

				if (m_LBTiled[tm*m_numTilesU+tu].size() > 0) {
					m_minibatchSizes[tm*m_numLocalIndexesPerTileInCL*16+tu] = (m_LBTiled[tm*m_numTilesU+tu].size() << 16) | m_numFeaturesInCL;
				}
				else {
					m_minibatchSizes[tm*m_numLocalIndexesPerTileInCL*16+tu] = 0;
				}

				uint32_t M_min = tm*m_tileSize;
				uint32_t U_min = tu*m_tileSize;

				for (uint32_t i = 0; i < m_MindexesChunks[tm*m_numTilesU+tu].m_lengthInCL*16; i++) {
					if (i < m_LBTiled[tm*m_numTilesU+tu].size()) {
						m_MindexesPtr[tm*m_numTilesU+tu][i] = ((m_numFeaturesInCL&0x3FFF) << 16) | ((m_LBTiled[tm*m_numTilesU+tu][i].m_Mindex-M_min)*m_numFeaturesInCL & 0x3FFF);
						// if ( tu%2 == 0 ) {
						// 	m_UindexesPtr[tm*m_numTilesU+tu][i] = ((m_numFeaturesInCL&0x3FFF) << 16) |
						// 	((m_LBTiled[tm*m_numTilesU+tu][i].m_Uindex-U_min)*m_numFeaturesInCL & 0x3FFF);
						// }
						// else {
						// 	m_UindexesPtr[tm*m_numTilesU+tu][i] = ((m_numFeaturesInCL&0x3FFF) << 16) |
						// 	((m_LBTiled[tm*m_numTilesU+tu][i].m_Uindex-U_min + m_tileSize)*m_numFeaturesInCL & 0x3FFF);
						// }

						m_UindexesPtr[tm*m_numTilesU+tu][i] = ((m_numFeaturesInCL&0x3FFF) << 16) |
						((m_LBTiled[tm*m_numTilesU+tu][i].m_Uindex-U_min)*m_numFeaturesInCL & 0x3FFF);

						m_ValuesPtr[tm*m_numTilesU+tu][i] = m_LBTiled[tm*m_numTilesU+tu][i].m_value;
					}
					else {
						m_MindexesPtr[tm*m_numTilesU+tu][i] = 0;
						m_UindexesPtr[tm*m_numTilesU+tu][i] = 0;
						m_ValuesPtr[tm*m_numTilesU+tu][i] = 0;
					}
				}
			}
		}

		return countCL;
	}

	bool CheckMemoryFit(uint32_t requestedSize, uint32_t onchipSize, const char* name) {
		if (requestedSize > onchipSize) {
			cout << "requestedSize for memory " << name << " (" << requestedSize << ")";
			cout << " is larger than onchipSize (" << onchipSize << endl;
			return false;
		}
		else {
			return true;
		}
	}
/*
	bool fOptimizeRound(
		uint32_t MtileToStart,
		uint32_t numTilesM,
		uint32_t UtileToStart,
		uint32_t numTilesU,
		float stepSize,
		float lambda,
		bool asyncUpdate,
		bool staleRead,
		uint32_t numEpochs)
	{
		if (m_base == nullptr) {
			cout << "m_base is nullptr!" << endl;
			return false;
		}

		bool fit = true;

		// MEM_ACCESSPROPS
		uint32_t accessMindexesOffsetInBRAM = 0;
		uint32_t accessUindexesOffsetInBRAM = accessMindexesOffsetInBRAM + m_numAccessIndexesPerTileInCL;
		uint32_t accessValuesOffsetInBRAM = accessUindexesOffsetInBRAM + m_numAccessIndexesPerTileInCL;
		uint32_t MEM_ACCESSPROPS_size = accessValuesOffsetInBRAM + m_numAccessIndexesPerTileInCL;
		fit &= CheckMemoryFit(MEM_ACCESSPROPS_size, "MEM_ACCESSPROPS");

		// MEM_LOCALPROPS
		uint32_t minibatchSizesOffsetInBRAM = 0;
		uint32_t MindexesOffsetInBRAM = m_numLocalIndexesPerTileInCL;
		uint32_t UindexesOffsetInBRAM = MindexesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(uint32_t));
		uint32_t MEM_LOCALPROPS_size = UindexesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(uint32_t));
		fit &= CheckMemoryFit(MEM_LOCALPROPS_size, "MEM_LOCALPROPS");

		// REGION_LABELS
		uint32_t ValuesOffsetInBRAM = 0;
		uint32_t Values2OffsetInBRAM = ValuesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(float));
		uint32_t REGION_LABELS_size = Values2OffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(float));
		fit &= CheckMemoryFit(REGION_LABELS_size, "REGION_LABELS");

		// REGION_MODEL
		uint32_t MtileOffsetInBRAM = 0;
		uint32_t REGION_MODEL_size = MtileOffsetInBRAM + m_tileSize*m_numFeaturesInCL;
		fit &= CheckMemoryFit(REGION_MODEL_size, "REGION_MODEL");

		// REGION_INPUT
		uint32_t UtileOffsetInBRAM = 0;
		uint32_t Utile2OffsetInBRAM = UtileOffsetInBRAM + m_tileSize*m_numFeaturesInCL;
		uint32_t REGION_INPUT_size = Utile2OffsetInBRAM + m_tileSize*m_numFeaturesInCL;
		fit &= CheckMemoryFit(REGION_INPUT_size, "REGION_INPUT");

		if (fit == false) {
			return false;
		}

		cout << "ConvertNumWordToNumCL(m_maxBatchSize): " << ConvertNumWordToNumCL(m_maxBatchSize, sizeof(uint32_t)) << endl;
		cout << "MEM_ACCESSPROPS_size: " << MEM_ACCESSPROPS_size << endl;
		cout << "MEM_LOCALPROPS_size: " << MEM_LOCALPROPS_size << endl;
		cout << "REGION_LABELS_size: " << REGION_LABELS_size << endl;
		cout << "REGION_MODEL_size: " << REGION_MODEL_size << endl;
		cout << "REGION_INPUT_size: " << REGION_INPUT_size << endl;

		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		m_inst[pc].SetIndex(0, UtileToStart);
		m_inst[pc].SetIndex(1, MtileToStart);
		m_inst[pc].ResetIndex(2);
		pc++;

		// Per M tile
		uint32_t loadMtile = pc;
		localaccess_t loadAccessMIndexWrite(BRAM, accessMindexesOffsetInBRAM, m_numAccessIndexesPerTileInCL);
		m_inst[pc].Load(m_accessMindexesChunk.m_offsetInCL, m_numAccessIndexesPerTileInCL,
			0, m_numAccessIndexesPerTileInCL, 0, loadAccessMIndexWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
		pc++;

		localaccess_t loadAccessUIndexWrite(BRAM, accessUindexesOffsetInBRAM, m_numAccessIndexesPerTileInCL);
		m_inst[pc].Load(m_accessUindexesChunk.m_offsetInCL, m_numAccessIndexesPerTileInCL,
			0, m_numAccessIndexesPerTileInCL, 0, loadAccessUIndexWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
		pc++;

		localaccess_t loadAccessValuesWrite(BRAM, accessValuesOffsetInBRAM, m_numAccessIndexesPerTileInCL);
		m_inst[pc].Load(m_accessValuesChunk.m_offsetInCL, m_numAccessIndexesPerTileInCL,
			0, m_numAccessIndexesPerTileInCL, 0, loadAccessValuesWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
		pc++;

		localaccess_t loadMinibatchSizesWrite(BRAM, minibatchSizesOffsetInBRAM, m_numLocalIndexesPerTileInCL);
		m_inst[pc].Load(m_minibatchSizesChunk.m_offsetInCL, m_numLocalIndexesPerTileInCL,
			0, m_numLocalIndexesPerTileInCL, 0, loadMinibatchSizesWrite, Instruction::LOAD_MEM_LOCALPROPS_CHANNEL);
		pc++;

		localaccess_t loadMTileWrite(BRAM, MtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
		m_inst[pc].Load(m_Mchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
			0, m_tileSize*m_numFeaturesInCL, 0, loadMTileWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
		pc++;

		// First load of values and U tile (the next loads will be in parallel to computation)
		localaccess_t loadValuesWrite(BRAM, ValuesOffsetInBRAM, 0);
		m_inst[pc].LocalLoad(accessValuesOffsetInBRAM,
			1, 0, 0, loadValuesWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
		pc++;

		localaccess_t loadUTileWrite(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
		m_inst[pc].Load(m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
			m_tileSize*m_numFeaturesInCL, 0, 0, loadUTileWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
		pc++;

		// Start---Innermost loop
			uint32_t startInnerMost = pc;
			m_inst[pc].LoadReg(3, minibatchSizesOffsetInBRAM);
			pc++;

			// Jump instruction here if reg[3] is 0
			uint32_t jumpIfSizeIsZero = pc;
			pc++;

			localaccess_t copyModelRead(BRAM, MtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
			m_inst[pc].Copy(copyModelRead, copyModelRead);
			m_inst[pc].MakeNonBlocking();
			pc++;

			// Start---Copy U tile to buffer 0 or buffer 1
				m_inst[pc].JumpIfEven(0, pc+1, pc+3);
				pc++;

				localaccess_t inputRead(BRAM, Utile2OffsetInBRAM, m_tileSize*m_numFeaturesInCL);
				m_inst[pc].Copy2(inputRead, inputRead);
				m_inst[pc].MakeNonBlocking();
				pc++;

				m_inst[pc].Jump(0, 0xFFFFFFFF, pc+2, 0);
				pc++;

				localaccess_t inputRead2(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
				m_inst[pc].Copy2(inputRead2, inputRead2);
				m_inst[pc].MakeNonBlocking();
				pc++;
			// End---Copy U tile to buffer 0 or buffer 1

			localaccess_t loadMindexesWrite(BRAM, MindexesOffsetInBRAM, 0);
			m_inst[pc].LocalLoad(accessMindexesOffsetInBRAM,
				1, 0, 0, loadMindexesWrite, Instruction::LOAD_MEM_LOCALPROPS_CHANNEL);
			pc++;

			localaccess_t loadUindexesWrite(BRAM, UindexesOffsetInBRAM, 0);
			m_inst[pc].LocalLoad(accessUindexesOffsetInBRAM,
				1, 0, 0, loadUindexesWrite, Instruction::LOAD_MEM_LOCALPROPS_CHANNEL);
			pc++;

			// Start---Load U tile to buffer 0 or buffer 1
				m_inst[pc].Jump(0, UtileToStart+numTilesU-1, pc+1, pc+5);
				pc++;

				m_inst[pc].JumpIfEven(0, pc+1, pc+3);
				m_inst[pc].IncrementIndex(0);
				pc++;

				loadUTileWrite.Set(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
				m_inst[pc].Load(m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
					m_tileSize*m_numFeaturesInCL, 0, 0, loadUTileWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
				m_inst[pc].MakeNonBlocking();
				m_inst[pc].DecrementIndex(0);
				pc++;

				m_inst[pc].Jump(0, 0xFFFFFFFF, pc+2, 0);
				pc++;

				loadUTileWrite.Set(BRAM, Utile2OffsetInBRAM, m_tileSize*m_numFeaturesInCL);
				m_inst[pc].Load(m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
					m_tileSize*m_numFeaturesInCL, 0, 0, loadUTileWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
				m_inst[pc].MakeNonBlocking();
				m_inst[pc].DecrementIndex(0);
				pc++;
			// End---Load U tile to buffer 0 or buffer 1

			localaccess_t dotLeftRead(BRAM, UindexesOffsetInBRAM, 1, true, true);
			localaccess_t dotRightRead(BRAM, MindexesOffsetInBRAM, 1, true, true);
			localaccess_t dotWrite(FIFO, 1);
			m_inst[pc].Dot(Instruction::USE_REG, m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, false);
			m_inst[pc].MakeNonBlocking();
			pc++;

			// Start---use values from buffer 0 or buffer 1
				localaccess_t labelsRead(BRAM, ValuesOffsetInBRAM, 1);
				localaccess_t labels2Read(BRAM, Values2OffsetInBRAM, 1);
				localaccess_t modifyWrite(FIFO, 1);

				m_inst[pc].JumpIfEven(0, pc+1, pc+3);
				pc++;

				m_inst[pc].Modify(false, Instruction::USE_REG, 0, 0, stepSize, lambda, labels2Read, modifyWrite);
				m_inst[pc].MakeNonBlocking();
				pc++;

				m_inst[pc].Jump(0, 0xFFFFFFFF, pc+2, 0);
				pc++;

				m_inst[pc].Modify(false, Instruction::USE_REG, 0, 0, stepSize, lambda, labelsRead, modifyWrite);
				m_inst[pc].MakeNonBlocking();
				pc++;
			// End---use values from buffer 0 or buffer 1

			localaccess_t updateURead(BRAM, UindexesOffsetInBRAM, 1, true, true);
			localaccess_t updateUWrite(BRAM, UindexesOffsetInBRAM, 1, true, true);
			localaccess_t updateMRead(BRAM, MindexesOffsetInBRAM, 1, true, true);
			localaccess_t updateMWrite(BRAM, MindexesOffsetInBRAM, 1, true, true);

			if (!staleRead) {
				m_inst[pc].BlockOnInstruction("Load");
				pc++;

				m_inst[pc].WriteForward1(Instruction::USE_REG, m_numFeaturesInCL);
				m_inst[pc].MakeNonBlocking();
				pc++;

				m_inst[pc].WriteForward2(Instruction::USE_REG, m_numFeaturesInCL);
				m_inst[pc].MakeNonBlocking();
				pc++;
			}

			m_inst[pc].Update(Instruction::USE_REG, m_numFeaturesInCL, updateURead, modifyWrite, updateMRead, updateMWrite, asyncUpdate);
			m_inst[pc].MakeNonBlocking();
			pc++;

			m_inst[pc].Update2(Instruction::USE_REG, m_numFeaturesInCL, updateMRead, modifyWrite, updateURead, updateUWrite, asyncUpdate);
			m_inst[pc].MakeNonBlocking();
			pc++;

			// Start---Load values to buffer 0 or buffer 1
				m_inst[pc].Jump(0, UtileToStart+numTilesU-1, pc+1, pc+5);
				pc++;

				m_inst[pc].JumpIfEven(0, pc+1, pc+3);
				m_inst[pc].IncrementIndex(0);
				pc++;

				loadValuesWrite.Set(BRAM, ValuesOffsetInBRAM, 0);
				m_inst[pc].LocalLoad(accessValuesOffsetInBRAM,
					1, 0, 0, loadValuesWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
				m_inst[pc].DecrementIndex(0);
				m_inst[pc].MakeNonBlocking();
				pc++;

				m_inst[pc].Jump(0, 0xFFFFFFFF, pc+2, 0);
				pc++;

				loadValuesWrite.Set(BRAM, Values2OffsetInBRAM, 0);
				m_inst[pc].LocalLoad(accessValuesOffsetInBRAM,
					1, 0, 0, loadValuesWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
				m_inst[pc].DecrementIndex(0);
				m_inst[pc].MakeNonBlocking();
				pc++;
			// End---Load values to buffer 0 or buffer 1

			m_inst[pc].BlockOnInstruction("Update2");
			pc++;

			// Start---use U tile from buffer 0 or buffer 1
				localaccess_t writebackURead(BRAM, Utile2OffsetInBRAM, m_tileSize*m_numFeaturesInCL);

				m_inst[pc].JumpIfEven(0, pc+1, pc+3);
				pc++;

				writebackURead.Set(BRAM, Utile2OffsetInBRAM, m_tileSize*m_numFeaturesInCL);
				m_inst[pc].WriteBack(true, m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
					m_tileSize*m_numFeaturesInCL, 0, 0,
					false, writebackURead, Instruction::WRITEBACK_INPUT_CHANNEL);
				m_inst[pc].MakeNonBlocking();
				pc++;

				m_inst[pc].Jump(0, 0xFFFFFFFF, pc+2, 0);
				pc++;

				writebackURead.Set(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
				m_inst[pc].WriteBack(true, m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
					m_tileSize*m_numFeaturesInCL, 0, 0,
					false, writebackURead, Instruction::WRITEBACK_INPUT_CHANNEL);
				m_inst[pc].MakeNonBlocking();
				pc++;
			// End---use U tile from buffer 0 or buffer 1

			uint32_t endInnerMost = pc;
			m_inst[pc].Jump(0, UtileToStart+numTilesU-1, startInnerMost, pc+1);
			m_inst[pc].IncrementIndex(0);
			pc++;

			m_inst[jumpIfSizeIsZero].Jump(3, 0, jumpIfSizeIsZero+1, endInnerMost);
		// End---Innermost loop

		localaccess_t writebackMRead(BRAM, MtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
		m_inst[pc].WriteBack(true, m_Mchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
			0, m_tileSize*m_numFeaturesInCL, 0,
			true, writebackMRead, Instruction::WRITEBACK_MODEL_CHANNEL);
		m_inst[pc].SetIndex(0, UtileToStart);
		m_inst[pc].IncrementIndex(1);
		m_inst[pc].Jump(1, MtileToStart+numTilesM-1, loadMtile, pc+1);
		pc++;

		m_inst[pc].Jump(2, numEpochs-1, loadMtile, 0xFFFFFFFF);
		m_inst[pc].SetIndex(0, UtileToStart);
		m_inst[pc].SetIndex(1, MtileToStart);
		m_inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************
		m_outputSizeInCL = 1;
		m_ifpga->Realloc(m_outputHandle, m_outputSizeInCL*64);

		m_numInstructions = pc;
		WriteProgramMemory(0, 0);

		return true;
	}
*/
	bool fOptimizeRound2(
		uint32_t MtileToStart,
		uint32_t numTilesM,
		uint32_t UtileToStart,
		uint32_t numTilesU,
		float stepSize,
		float lambda,
		bool asyncUpdate,
		bool staleRead,
		uint32_t numEpochs)
	{
		if (m_base == nullptr) {
			cout << "m_base is nullptr!" << endl;
			return false;
		}

		bool fit = true;

		// MEM_ACCESSPROPS
		uint32_t accessMindexesOffsetInBRAM = 0;
		uint32_t accessUindexesOffsetInBRAM = accessMindexesOffsetInBRAM + m_numAccessIndexesPerTileInCL;
		uint32_t accessValuesOffsetInBRAM = accessUindexesOffsetInBRAM + m_numAccessIndexesPerTileInCL;
		uint32_t MEM_ACCESSPROPS_size = accessValuesOffsetInBRAM + m_numAccessIndexesPerTileInCL;
		fit &= CheckMemoryFit(MEM_ACCESSPROPS_size, iFPGA::MEMORY_SIZE_IN_CL, "MEM_ACCESSPROPS");

		// MEM_LOCALPROPS
		uint32_t minibatchSizesOffsetInBRAM = 0;
		uint32_t MindexesOffsetInBRAM = m_numLocalIndexesPerTileInCL;
		uint32_t UindexesOffsetInBRAM = MindexesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(uint32_t));
		uint32_t MEM_LOCALPROPS_size = UindexesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(uint32_t));
		fit &= CheckMemoryFit(MEM_LOCALPROPS_size, iFPGA::MEMORY_SIZE_IN_CL, "MEM_LOCALPROPS");

		// REGION_LABELS
		uint32_t ValuesOffsetInBRAM = 0;
		uint32_t REGION_LABELS_size = ValuesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(float));
		fit &= CheckMemoryFit(REGION_LABELS_size, iFPGA::MEMORY_SIZE_IN_CL, "REGION_LABELS");

		// REGION_MODEL
		uint32_t MtileOffsetInBRAM = 0;
		uint32_t REGION_MODEL_size = MtileOffsetInBRAM + m_tileSize*m_numFeaturesInCL;
		fit &= CheckMemoryFit(REGION_MODEL_size, iFPGA::MEMORY_SIZE_IN_CL, "REGION_MODEL");

		// REGION_INPUT
		uint32_t UtileOffsetInBRAM = 0;
		uint32_t REGION_INPUT_size = UtileOffsetInBRAM + m_tileSize*m_numFeaturesInCL;
		fit &= CheckMemoryFit(REGION_INPUT_size, iFPGA::MEMORY_SIZE_IN_CL, "REGION_INPUT");

		// REGION PREFECT
		uint32_t PrefetchUtileOffsetInBRAM = 0;
		uint32_t PrefetchValuesOffsetInBRAM = PrefetchUtileOffsetInBRAM + m_tileSize*m_numFeaturesInCL;
		uint32_t REGION_PREFETCH_size = PrefetchValuesOffsetInBRAM + ConvertNumWordToNumCL(m_maxBatchSize, sizeof(float));
		fit &= CheckMemoryFit(REGION_PREFETCH_size, iFPGA::PREFETCH_BUFFER_SIZE_IN_CL, "REGION_PREFETCH");

		if (fit == false) {
			return false;
		}

		cout << "ConvertNumWordToNumCL(m_maxBatchSize): " << ConvertNumWordToNumCL(m_maxBatchSize, sizeof(uint32_t)) << endl;
		cout << "MEM_ACCESSPROPS_size: " << MEM_ACCESSPROPS_size << endl;
		cout << "MEM_LOCALPROPS_size: " << MEM_LOCALPROPS_size << endl;
		cout << "REGION_LABELS_size: " << REGION_LABELS_size << endl;
		cout << "REGION_MODEL_size: " << REGION_MODEL_size << endl;
		cout << "REGION_INPUT_size: " << REGION_INPUT_size << endl;
		cout << "REGION_PREFETCH_size: " << REGION_PREFETCH_size << endl;

		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		m_inst[pc].SetIndex(0, UtileToStart);
		m_inst[pc].SetIndex(1, MtileToStart);
		m_inst[pc].ResetIndex(2);
		pc++;

		// Per M tile
		uint32_t loadMtile = pc;
		localaccess_t loadAccessMIndexWrite(BRAM, accessMindexesOffsetInBRAM, m_numAccessIndexesPerTileInCL);
		m_inst[pc].Load(m_accessMindexesChunk.m_offsetInCL, m_numAccessIndexesPerTileInCL,
			0, m_numAccessIndexesPerTileInCL, 0, loadAccessMIndexWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
		pc++;

		localaccess_t loadAccessUIndexWrite(BRAM, accessUindexesOffsetInBRAM, m_numAccessIndexesPerTileInCL);
		m_inst[pc].Load(m_accessUindexesChunk.m_offsetInCL, m_numAccessIndexesPerTileInCL,
			0, m_numAccessIndexesPerTileInCL, 0, loadAccessUIndexWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
		pc++;

		localaccess_t loadAccessValuesWrite(BRAM, accessValuesOffsetInBRAM, m_numAccessIndexesPerTileInCL);
		m_inst[pc].Load(m_accessValuesChunk.m_offsetInCL, m_numAccessIndexesPerTileInCL,
			0, m_numAccessIndexesPerTileInCL, 0, loadAccessValuesWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
		pc++;

		localaccess_t loadMinibatchSizesWrite(BRAM, minibatchSizesOffsetInBRAM, m_numLocalIndexesPerTileInCL);
		m_inst[pc].Load(m_minibatchSizesChunk.m_offsetInCL, m_numLocalIndexesPerTileInCL,
			0, m_numLocalIndexesPerTileInCL, 0, loadMinibatchSizesWrite, Instruction::LOAD_MEM_LOCALPROPS_CHANNEL);
		pc++;

		localaccess_t loadMTileWrite(BRAM, MtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
		m_inst[pc].Load(m_Mchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
			0, m_tileSize*m_numFeaturesInCL, 0, loadMTileWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
		pc++;

		// First load of values and U tile (the next loads will be in parallel to computation)
		localaccess_t loadUTileWrite(BRAM, PrefetchUtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
		m_inst[pc].Load(m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
			m_tileSize*m_numFeaturesInCL, 0, 0, loadUTileWrite, Instruction::LOAD_REGION_PREFETCH_CHANNEL);
		pc++;

		localaccess_t loadValuesWrite(BRAM, PrefetchValuesOffsetInBRAM, 0);
		m_inst[pc].LocalLoad(accessValuesOffsetInBRAM,
			1, 0, 0, loadValuesWrite, Instruction::LOAD_REGION_PREFETCH_CHANNEL);
		pc++;

		// Start---Innermost loop
			uint32_t startInnerMost = pc;
			m_inst[pc].LoadReg(3, minibatchSizesOffsetInBRAM);
			pc++;

			// Jump instruction here if reg[3] is 0
			uint32_t jumpIfSizeIsZero = pc;
			pc++;

			localaccess_t copyModelRead(BRAM, MtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
			m_inst[pc].Copy(copyModelRead, copyModelRead);
			m_inst[pc].MakeNonBlocking();
			pc++;

			localaccess_t loadMindexesWrite(BRAM, MindexesOffsetInBRAM, 0);
			m_inst[pc].LocalLoad(accessMindexesOffsetInBRAM,
				1, 0, 0, loadMindexesWrite, Instruction::LOAD_MEM_LOCALPROPS_CHANNEL);
			m_inst[pc].MakeNonBlocking();
			pc++;

			localaccess_t valuesRead(BRAM, PrefetchValuesOffsetInBRAM, ConvertNumWordToNumCL(m_maxBatchSize, sizeof(float)));
			localaccess_t valuesWrite(BRAM, ValuesOffsetInBRAM, ConvertNumWordToNumCL(m_maxBatchSize, sizeof(float)));
			m_inst[pc].CopyPrefetch(valuesRead, valuesWrite, Instruction::PREFETCH_REGION_LABELS_CHANNEL);
			m_inst[pc].MakeNonBlocking();
			pc++;

			localaccess_t loadUindexesWrite(BRAM, UindexesOffsetInBRAM, 0);
			m_inst[pc].LocalLoad(accessUindexesOffsetInBRAM,
				1, 0, 0, loadUindexesWrite, Instruction::LOAD_MEM_LOCALPROPS_CHANNEL);
			m_inst[pc].MakeNonBlocking();
			pc++;

			m_inst[pc].BlockOnInstruction("WriteBack");
			pc++;

			localaccess_t inputRead(BRAM, PrefetchUtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
			localaccess_t inputWrite(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
			m_inst[pc].CopyPrefetch(inputRead, inputWrite, Instruction::PREFETCH_REGION_INPUT_CHANNEL | Instruction::PREFETCH_REGION_INPUTCOPY_CHANNEL);
			pc++;

			m_inst[pc].BlockOnInstruction("Load");
			pc++;

			localaccess_t dotLeftRead(BRAM, UindexesOffsetInBRAM, 1, true, true);
			localaccess_t dotRightRead(BRAM, MindexesOffsetInBRAM, 1, true, true);
			localaccess_t dotWrite(FIFO, 1);
			m_inst[pc].Dot(Instruction::USE_REG, m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, false);
			m_inst[pc].MakeNonBlocking();
			pc++;

			localaccess_t labelsRead(BRAM, ValuesOffsetInBRAM, 1);
			localaccess_t modifyWrite(FIFO, 1);
			m_inst[pc].Modify(false, Instruction::USE_REG, 0, 0, stepSize, lambda, labelsRead, modifyWrite);
			m_inst[pc].MakeNonBlocking();
			pc++;

			localaccess_t updateURead(BRAM, UindexesOffsetInBRAM, 1, true, true);
			localaccess_t updateUWrite(BRAM, UindexesOffsetInBRAM, 1, true, true);
			localaccess_t updateMRead(BRAM, MindexesOffsetInBRAM, 1, true, true);
			localaccess_t updateMWrite(BRAM, MindexesOffsetInBRAM, 1, true, true);

			if (!staleRead) {
				m_inst[pc].WriteForward1(Instruction::USE_REG, m_numFeaturesInCL);
				m_inst[pc].MakeNonBlocking();
				pc++;

				m_inst[pc].WriteForward2(Instruction::USE_REG, m_numFeaturesInCL);
				m_inst[pc].MakeNonBlocking();
				pc++;
			}

			m_inst[pc].Update(Instruction::USE_REG, m_numFeaturesInCL, updateURead, modifyWrite, updateMRead, updateMWrite, asyncUpdate);
			m_inst[pc].MakeNonBlocking();
			pc++;

			m_inst[pc].Update2(Instruction::USE_REG, m_numFeaturesInCL, updateMRead, modifyWrite, updateURead, updateUWrite, asyncUpdate);
			m_inst[pc].MakeNonBlocking();
			pc++;

			m_inst[pc].Jump(0, UtileToStart+numTilesU-1, pc+1, pc+3);
			m_inst[pc].IncrementIndex(0);
			pc++;

			m_inst[pc].Load(m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
				m_tileSize*m_numFeaturesInCL, 0, 0, loadUTileWrite, Instruction::LOAD_REGION_PREFETCH_CHANNEL);
			m_inst[pc].MakeNonBlocking();
			pc++;

			m_inst[pc].LocalLoad(accessValuesOffsetInBRAM,
				1, 0, 0, loadValuesWrite, Instruction::LOAD_REGION_PREFETCH_CHANNEL);
			m_inst[pc].MakeNonBlocking();
			pc++;

			m_inst[pc].BlockOnInstruction("Update2");
			m_inst[pc].DecrementIndex(0);
			pc++;

			localaccess_t writebackURead(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
			writebackURead.Set(BRAM, UtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
			m_inst[pc].WriteBack(true, m_Uchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
				m_tileSize*m_numFeaturesInCL, 0, 0,
				false, writebackURead, Instruction::WRITEBACK_INPUT_CHANNEL);
			m_inst[pc].MakeNonBlocking();
			pc++;

			uint32_t endInnerMost = pc;
			m_inst[pc].Jump(0, UtileToStart+numTilesU-1, startInnerMost, pc+1);
			m_inst[pc].IncrementIndex(0);
			pc++;

			m_inst[jumpIfSizeIsZero].Jump(3, 0, jumpIfSizeIsZero+1, endInnerMost);
		// End---Innermost loop

		localaccess_t writebackMRead(BRAM, MtileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);
		m_inst[pc].WriteBack(true, m_Mchunk.m_offsetInCL, m_tileSize*m_numFeaturesInCL,
			0, m_tileSize*m_numFeaturesInCL, 0,
			true, writebackMRead, Instruction::WRITEBACK_MODEL_CHANNEL);
		m_inst[pc].SetIndex(0, UtileToStart);
		m_inst[pc].IncrementIndex(1);
		m_inst[pc].Jump(1, MtileToStart+numTilesM-1, loadMtile, pc+1);
		pc++;

		m_inst[pc].Jump(2, numEpochs-1, loadMtile, 0xFFFFFFFF);
		m_inst[pc].SetIndex(0, UtileToStart);
		m_inst[pc].SetIndex(1, MtileToStart);
		m_inst[pc].IncrementIndex(2);
		pc++;

		// *************************************************************************
		//
		//   END Program
		//
		// *************************************************************************
		m_outputSizeInCL = 1;
		m_ifpga->Realloc(m_outputHandle, m_outputSizeInCL*64);

		m_numInstructions = pc;
		WriteProgramMemory(0, 0);

		return true;
	}
};