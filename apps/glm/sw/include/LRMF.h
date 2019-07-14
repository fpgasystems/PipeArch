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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/time.h>
#include <dirent.h>
#include <cmath>
#include <algorithm>
#include <thread>
#include "ColumnStore.h" // get_time()

using namespace std;

struct Label {
	int m_Uindex;
	int m_value;
};

struct LabelB {
	int m_Uindex;
	int m_Mindex;
	int m_value;
};

class LRMF {
protected:
	uint32_t m_numFeatures;
	uint32_t m_Mdim;
	uint32_t m_Udim;

	float* m_M; // m_Mdim x m_numFeatures
	float* m_U; // m_Udim x m_numFeatures
	vector< vector<Label> > m_L;
	vector< LabelB > m_LB;
	vector< vector< LabelB > > m_LBTiled;

	uint32_t m_numTilesM;
	uint32_t m_numTilesU;
	uint32_t m_tileSize;

	int CountFiles(char* pathToDirectory) {
		auto dir = opendir(pathToDirectory);
		if (dir == NULL) {
			cout << "Could not open directory: " << pathToDirectory << endl;
			return -1;
		}

		int count = 0;
		auto entity = readdir(dir);
		while (entity != NULL) {
			count++;
			entity = readdir(dir);
		}

		return count;
	}

	void deallocData() {
		// dealloc if not nullptr
		if (m_M != nullptr) {
			free(m_M);
		}
		if (m_U != nullptr) {
			free(m_U);
		}
	}

	uint32_t RandRange(uint32_t max) {
		float temp = (float)rand()/(float)RAND_MAX;
		return (uint32_t)((float)max*temp);
	}

	void RandInit(float* base, uint32_t length) {
		for (uint32_t i = 0; i < length; i++) {
			// base[i] = (float)rand()/(float)RAND_MAX/(float)m_numFeatures;
			base[i] = (float)rand()/(float)RAND_MAX;
		}
	}

public:
	uint32_t GetNumTilesM() {return m_numTilesM;}
	uint32_t GetNumTilesU() {return m_numTilesU;}
	uint32_t GetTileSize() {return m_tileSize;}

	LRMF(uint32_t numFeatures) {
		m_numFeatures = numFeatures;
		m_Mdim = 0;
		m_Udim = 0;
		m_M = nullptr;
		m_U = nullptr;
	}

	~LRMF() {
		if (m_numFeatures > 0)
			deallocData();
	}

	void RandInitMU() {
		srand(3);
		RandInit(m_M, m_Mdim*m_numFeatures);
		memset(m_U, 0, m_Udim*m_numFeatures*sizeof(float));
	}

	void PrintLB() {
		for (uint32_t i = 0; i < m_LB.size(); i++) {
			cout << "m_LB[" << i << "]: " << m_LB[i].m_Mindex << "\t";
			cout << m_LB[i].m_Uindex << "\t" << m_LB[i].m_value << endl;
		}
	}

	void PrintM(){
		cout << "m_M:" << endl;
		for (uint32_t m = 0; m < m_Mdim; m++) {
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				cout << m_M[m*m_numFeatures + j] << " ";
			}
			cout << endl;
		}
	}

	void PrintU(){
		cout << "m_U:" << endl;
		for (uint32_t u = 0; u < m_Udim; u++) {
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				cout << m_U[u*m_numFeatures + j] << " ";
			}
			cout << endl;
		}
	}

	size_t GetDataSize() {
		return (m_Mdim+m_Udim)*m_numFeatures*sizeof(float) + m_LB.size()*sizeof(LabelB);
	}

	void ReadNetflixData(char* pathToFile, int Mdim, int Udim);
	void GenerateSyntheticData(int Mdim, uint32_t Udim, float sparsenessFactor);
	void CopyDataset(LRMF* lrmf);
	void DivideLBIntoTiles(uint32_t tileSize);
	float Loss(float lambda);
	float RMSE();
	void OptimizeLR(float stepSize, float lambda, uint32_t numEpochs);
	void OptimizeNaive(float stepSize, float lambda, uint32_t numEpochs);
	void Optimize(float stepSize, float lambda, uint32_t numEpochs);
	void OptimizeRound(float stepSize, float lambda, uint32_t numEpochs);
	void OptimizeRoundStale(float stepSize, float lambda, uint32_t numEpochs);

	void DivideWorkByThread(
		uint32_t numThreads,
		vector<uint32_t>& MtileToStart,
		vector<uint32_t>& numTilesM,
		vector<uint32_t>& UtileToStart,
		vector<uint32_t>& numTilesU);

	static float Dot(float* vector1, float* vector2, uint32_t numFeatures) {
		float dot = 0.0;
		for (uint32_t j = 0; j < numFeatures; j++) {
			dot += vector1[j]*vector2[j];
		}
		return dot;
	}

	static void UpdateTile(
			uint32_t threadId,
			float* m_M,
			float* m_U,
			vector< vector<LabelB> >* m_LBTiled,
			uint32_t MtileToStart,
			uint32_t numTilesM,
			uint32_t UtileToStart,
			uint32_t numTilesU,
			uint32_t totalNumTilesU,
			uint32_t m_numFeatures,
			uint32_t m_tileSize,
			float stepSize,
			float lambda);

	void OptimizeRoundMulti(float stepSize, float lambda, uint32_t numEpochs, uint32_t numThreads);
};