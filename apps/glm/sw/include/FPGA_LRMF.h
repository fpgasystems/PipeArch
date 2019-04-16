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
#include "iFPGA.h"
#include "Instruction.h"

class FPGA_LRMF : public LRMF {
private:
	iFPGA* m_ifpga;

	volatile float* m_base = nullptr;
	volatile float* m_Mptr = nullptr;
	volatile float* m_Uptr = nullptr;
	volatile access_t* m_accessprops = nullptr;
	volatile LabelT** m_Lptr = nullptr;

	access_t m_Mchunk;
	access_t m_Uchunk;
	access_t m_accesspropsChunk;
	access_t** m_Lchunks;

public:
	iFPGA_ptr m_inputHandle;
	iFPGA_ptr m_outputHandle;
	iFPGA_ptr m_programMemoryHandle;

	uint32_t m_numFeaturesInCL;
	uint32_t m_alignedNumFeatures;
	uint32_t m_MdimInCL;
	uint32_t m_UdimInCL;
	uint32_t m_numLabelsInCL;
	uint32_t m_tileSize;
	uint32_t m_numTiles;
	uint32_t m_rest;

	FPGA_LRMF(iFPGA* ifpga) {
		m_ifpga = ifpga;
	}

	inline uint32_t ConvertNumWordToNumCL(uint32_t numWords) {
		return (numWords >> 4) + ((numWords&0xF) > 0);
	}

	uint32_t CreateMemoryLayout(uint32_t tileSize) {

		m_tileSize = (tileSize > m_Mdim) ? m_Mdim L : tileSize;
		m_numTiles = m_Mdim/m_tileSize;
		m_rest = m_Mdim - m_numTiles*m_tileSize;

		vector< vector< vector<LabelT> >  > Ltranspose;
		Ltranspose.resize(m_Udim);
		for (uint32_t u = 0; u < m_Udim; u++) {
			Ltranspose[u].resize(m_numTiles);
		}

		for (uint32_t t = 0; t < m_numTiles; t++) {
			for (uint32_t i = 0; i < m_tileSize; i++) {
				for (uint32_t u = 0; u < m_L[t*m_tileSize + i].size(); u++) {
					uint32_t uindex = m_L[t*m_tileSize + i][u].m_Uindex;
					LabelT temp;
					temp.m_Mindex = i;
					temp.m_value = m_L[t*m_tileSize + i][u].m_value;
					Ltranspose[uindex][t].push_back(temp);
				}
			}
		}

		m_numFeaturesInCL = ConvertNumWordToNumCL(m_numFeatures);
		m_alignedNumFeatures = m_numFeaturesInCL*16;
		m_MdimInCL = ConvertNumWordToNumCL(m_Mdim);
		m_UdimInCL = ConvertNumWordToNumCL(m_Udim);
		m_numLabelsInCL = 0;
		for (uint32_t u = 0; u < m_Udim; u++) {
			for (uint32_t t = 0; t < m_numTiles; t++) {
				m_numLabelsInCL += ConvertNumWordToNumCL( Ltranspose[u][t].size() );
			}
		}

		cout << "m_tileSize: " << m_tileSize << endl;
		cout << "m_numTiles: " << m_numTiles << endl;
		cout << "m_rest: " << m_rest << endl;

		uint32_t countCL = 0;

		m_Mchunk.m_offsetInCL = countCL;
		countCL += m_numFeaturesInCL*m_Mdim;
		m_Mchunk.m_lengthInCL = countCL - m_Mchunk.m_offsetInCL;

		m_Uchunk.m_offsetInCL = countCL;
		countCL += m_numFeaturesInCL*m_Udim;
		m_Uchunk.m_lengthInCL = countCL - m_Uchunk.m_offsetInCL;

		m_accesspropsChunk.m_offsetInCL = countCL;
		countCL += 2*ConvertNumWordToNumCL(m_Udim*m_numTiles); // *2, because offset and length
		m_accesspropsChunk.m_lengthInCL = countCL - m_accesspropsChunk.m_offsetInCL;

		m_Lchunks = new access_t*[m_Udim];
		for (uint32_t u = 0; u < m_Udim; u++) {
			m_Lchunks[u] = new access_t[m_numTiles];
			for (uint32_t t = 0; t < m_numTiles; t++) {
				m_Lchunks[u][t].m_offsetInCL = countCL;
				countCL += 2*ConvertNumWordToNumCL( Ltranspose[u][t].size() ); // *2, because LabelT is 64 bits
				m_Lchunks[u][t].m_lengthInCL = countCL - m_Lchunks[u][t].m_offsetInCL;
			}
		}

		m_ifpga->Realloc(m_inputHandle, countCL*64);
		m_base = iFPGA::CastToFloat(m_inputHandle);
		memset((void*)m_base, 0, 16*countCL*sizeof(float));

		m_Mptr = m_base + m_Mchunk.m_offsetInCL*16;
		m_Uptr = m_base + m_Uchunk.m_offsetInCL*16;
		m_accessprops = (access_t*)m_base + m_accesspropsChunk.m_offsetInCL*8;
		m_Lptr = new volatile LabelT*[m_Udim*m_numTiles];
		for (uint32_t u = 0; u < m_Udim; u++) {
			for (uint32_t t = 0; t < m_numTiles; t++) {
				m_Lptr[u*m_numTiles + t] = ((LabelT*)m_base) + m_Lchunks[u][t].m_offsetInCL*8;
			}
		}

		for (uint32_t u = 0; u < m_Udim; u++) {
			for (uint32_t t = 0; t < m_numTiles; t++) {
			
				m_accessprops[u*m_numTiles + t].m_offsetInCL = m_Lchunks[u][t].m_offsetInCL;
				m_accessprops[u*m_numTiles + t].m_lengthInCL = m_Lchunks[u][t].m_lengthInCL;

				for (uint32_t i = 0; i < m_Lchunks[u][t].m_lengthInCL*8; i++) {
					if (i < Ltranspose[u][t].size()) {
						m_Lptr[u*m_numTiles + t][i] = Ltranspose[u][t][i];
					}
					else {
						m_Lptr[u*m_numTiles + t][i].m_Mindex = 0;
						m_Lptr[u*m_numTiles + t][i].m_value = 0;
					}
				}
			}
		}

		return countCL;
	}

	bool OptimizeTiled(
		float stepSize,
		float lambda,
		uint32_t numEpochs)
	{
		uint32_t tileOffsetInBRAM = 0;
		uint32_t accesspropsOffsetInBRAM = 0;

		AccessProperties accessM(5);
		accessM.Set(2, tileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);

		AccessProperties accessAccessProps(5);
		accessAccessProps.Set(4, accesspropsOffsetInBRAM, m_numTiles);

		AccessProperties accessU(5);
		accessU.Set(0, 0, m_numFeaturesInCL);

		AccessProperties writebackU(2);
		writebackU.Set(0, 0, m_numFeaturesInCL);

		AccessProperties writebackM(2);
		writebackM.Set(0, tileOffsetInBRAM, m_tileSize*m_numFeaturesInCL);

		// *************************************************************************
		//
		//   START Program
		//
		// *************************************************************************
		uint32_t pc = 0;

		

	}
}