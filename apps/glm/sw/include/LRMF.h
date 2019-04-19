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
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/time.h>
#include <dirent.h>
#include <cmath>
#include <algorithm>
#include "ColumnStore.h"

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
	uint32_t m_restM;
	uint32_t m_restU;

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
			base[i] = (float)rand()/(float)RAND_MAX;
		}
	}

public:
	LRMF(uint32_t numFeatures) {
		m_numFeatures = numFeatures;
		m_Mdim = 0;
		m_Udim = 0;
		m_M = nullptr;
		m_U = nullptr;
	}

	~LRMF() {
		deallocData();
	}

	void RandInitMU() {
		srand(3);
		RandInit(m_M, m_Mdim*m_numFeatures);
		RandInit(m_U, m_Udim*m_numFeatures);
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

	void ReadNetflixData(char* pathToFile, int Mdim) {
		FILE* f = fopen(pathToFile, "r");
		if (f == NULL) {
			cout << "Can't find files at pathToFile" << endl;
			return;
		}

		deallocData();

		size_t readsize;

		readsize = fread(&m_Mdim, sizeof(uint32_t), 1, f);
		if (Mdim != -1) {
			m_Mdim = Mdim;
		}
		cout << "m_Mdim: " << m_Mdim << endl;
		m_L.reserve(m_Mdim);

		for (uint32_t i = 0; i < m_Mdim; i++) {
			// cout << "-------------------------------------------------" << endl;

			uint32_t Mindex = 0;
			readsize = fread(&Mindex, sizeof(uint32_t), 1, f);

			uint32_t numEntries = 0;
			readsize = fread(&numEntries, sizeof(uint32_t), 1, f);

			// cout << "Mindex: " << Mindex << endl;
			// cout << "numEntries: " << numEntries << endl;

			Label* temp = (Label*)malloc(sizeof(Label)*numEntries);
			readsize = fread(temp, sizeof(Label), numEntries, f);

			vector<Label> tempV;
			tempV.reserve(numEntries);

			for (uint32_t j = 0; j < numEntries; j++) {
				tempV.push_back(temp[j]);
				// cout << j << " User: " << m_L[i][j].m_Uindex << ", Rating: " << m_L[i][j].m_value << endl;

				if ((temp[j].m_Uindex+1) > m_Udim) {
					m_Udim = (temp[j].m_Uindex+1);
				}

				LabelB tempB;
				tempB.m_Uindex = temp[j].m_Uindex;
				tempB.m_Mindex = Mindex;
				tempB.m_value = temp[j].m_value;
				m_LB.push_back(tempB);
			}
			free(temp);

			m_L.push_back(tempV);
		}
		cout << "m_Udim: " << m_Udim << endl;

		m_M = (float*)aligned_alloc(64, m_Mdim*m_numFeatures*sizeof(float));
		m_U = (float*)aligned_alloc(64, m_Udim*m_numFeatures*sizeof(float));
		RandInitMU();
		m_LBTiled.clear();
	}

	void GenerateSyntheticData(int Mdim, uint32_t Udim) {
		srand(3);

		m_Mdim = Mdim;
		m_Udim = Udim;

		m_L.reserve(m_Mdim);
		for (uint32_t i = 0; i < m_Mdim; i++) {

			uint32_t numEntries = RandRange(m_Udim);
			// cout << "Mindex: " << i << endl;
			// cout << "numEntries: " << numEntries << endl;

			vector<Label> tempV;
			tempV.reserve(numEntries);
			for (uint32_t j = 0; j < numEntries; j++) {
				Label tempLabel;
				tempLabel.m_Uindex = RandRange(m_Udim);
				tempLabel.m_value = RandRange(5);
				tempV.push_back(tempLabel);

				LabelB tempB;
				tempB.m_Uindex = tempLabel.m_Uindex;
				tempB.m_Mindex = i;
				tempB.m_value = tempLabel.m_value;
				m_LB.push_back(tempB);
			}

			m_L.push_back(tempV);
		}

		cout << "m_Mdim: " << m_Mdim << endl;
		cout << "m_Udim: " << m_Udim << endl;

		m_M = (float*)aligned_alloc(64, m_Mdim*m_numFeatures*sizeof(float));
		m_U = (float*)aligned_alloc(64, m_Udim*m_numFeatures*sizeof(float));
		RandInitMU();
		m_LBTiled.clear();
	}

	void DivideLBIntoTiles(uint32_t tileSize) {
		srand(3);

		m_LBTiled.clear();

		m_tileSize = tileSize;
		m_numTilesM = m_Mdim/m_tileSize;
		m_numTilesU = m_Udim/m_tileSize;
		m_restM = m_Mdim - m_numTilesM*m_tileSize;
		m_restU = m_Udim - m_numTilesU*m_tileSize;

		cout << "m_numTilesM: " << m_numTilesM << endl;
		cout << "m_numTilesU: " << m_numTilesU << endl;
		cout << "m_restM: " << m_restM << endl;
		cout << "m_restU: " << m_restU << endl;

		uint32_t count = 0;
		for (uint32_t tm = 0; tm < m_numTilesM; tm++) {
			for (uint32_t tu = 0; tu < m_numTilesU; tu++) {
				vector<LabelB> temp;

				uint32_t M_min = tm*m_tileSize;
				uint32_t M_max = (tm+1)*m_tileSize-1;
				uint32_t U_min = tu*m_tileSize;
				uint32_t U_max = (tu+1)*m_tileSize-1;

				for (uint32_t i = 0; i < m_LB.size(); i++) {
					if (m_LB[i].m_Mindex >= M_min && m_LB[i].m_Mindex <= M_max && m_LB[i].m_Uindex >= U_min && m_LB[i].m_Uindex <= U_max) {
						temp.push_back(m_LB[i]);
					}
				}

				random_shuffle(temp.begin(), temp.end());
				m_LBTiled.push_back(temp);
				// cout << tm << ", " << tu << " m_LBTiled.size(): " << temp.size() << endl;
				count += temp.size();
			}
		}

		// cout << "count: " << count << endl;
		// cout << "m_LB.size(): " << m_LB.size() << endl;

		// for (uint32_t i = 0; i < m_LBTiled[0].size(); i++) {
		// 	cout << m_LBTiled[0][i].m_Mindex << "\t";
		// 	cout << m_LBTiled[0][i].m_Uindex << "\t";
		// 	cout << m_LBTiled[0][i].m_value << endl;
		// }
	}

	float Dot(float* vector1, float* vector2, uint32_t numFeatures) {
		float dot = 0.0;
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			dot += vector1[j]*vector2[j];
		}
		// cout << "----------------------------------------" << endl;
		// for (uint32_t j = 0; j < m_numFeatures; j++) {
		// 	cout << "vector1[" << j << "]: " << vector1[j] << endl;
		// }
		// for (uint32_t j = 0; j < m_numFeatures; j++) {
		// 	cout << "vector2[" << j << "]: " << vector2[j] << endl;
		// }
		// cout << "dot: " << dot << endl;
		return dot;
	}

	float Loss(float lambda) {
		float loss = 0.0;
		for (uint32_t m = 0; m < m_Mdim; m++) {
			float temploss = 0.0;
			for (uint32_t u = 0; u < m_L[m].size(); u++) {
				// cout << "m_L[" << m << "][" << u << "]: " << m_L[m][u].m_Uindex << " " << m_L[m][u].m_value << endl;

				float* M_vector = m_M + m*m_numFeatures;
				float* U_vector = m_U + m_L[m][u].m_Uindex*m_numFeatures;

				float dot = Dot(M_vector, U_vector, m_numFeatures);
				float error = (m_L[m][u].m_value - dot);
				temploss += error*error;
			}
			loss += temploss/m_L[m].size();
		}
		loss *= 0.5;

		float regularization = 0.0;
		for (uint32_t m = 0; m < m_Mdim; m++) {
			float* M_vector = m_M + m*m_numFeatures;
			regularization += 0.5*Dot(M_vector, M_vector, m_numFeatures);
		}

		for (uint32_t u = 0; u < m_Udim; u++) {
			float* U_vector = m_U + u*m_numFeatures;
			regularization += 0.5*Dot(U_vector, U_vector, m_numFeatures);
		}
		loss += lambda*regularization;

		return loss;
	}

	float RMSE() {
		float loss = 0.0;
		for (uint32_t m = 0; m < m_Mdim; m++) {
			float temploss = 0.0;
			float* M_vector = m_M + m*m_numFeatures;
			for (uint32_t u = 0; u < m_L[m].size(); u++) {
				float* U_vector = m_U + m_L[m][u].m_Uindex*m_numFeatures;

				float dot = Dot(M_vector, U_vector, m_numFeatures);
				float error = (m_L[m][u].m_value - dot);
				temploss += error*error;
			}
			loss += temploss/m_L[m].size();
		}
		loss /= m_Mdim;
		return sqrt(loss);
	}

	void Optimize(float stepSize, float lambda, uint32_t numEpochs) {

		// float loss = Loss(lambda);
		float loss = RMSE();
		cout << "Initial Loss: " << loss << endl;

		for (uint32_t e = 0; e < numEpochs; e++) {
			double start = get_time();

			for (uint32_t m = 0; m < m_Mdim; m++) {
				// cout << "m: " << m << endl;
				// cout << "m_L[m].size(): " << m_L[m].size() << endl;
				float* M_vector = m_M + m*m_numFeatures;

				for (uint32_t u = 0; u < m_L[m].size(); u++) {

					// cout << "m_L[m][u].m_Uindex: " << m_L[m][u].m_Uindex << endl;
					float* U_vector = m_U + m_L[m][u].m_Uindex*m_numFeatures;

					float dot = Dot(M_vector, U_vector, m_numFeatures);
					float error = dot - m_L[m][u].m_value;

					for (uint32_t j = 0; j < m_numFeatures; j++) {
						float M_temp = M_vector[j];
						float U_temp = U_vector[j];
						M_vector[j] = M_temp - stepSize*(error*U_temp + lambda*M_temp);
						U_vector[j] = U_temp - stepSize*(error*M_temp + lambda*U_temp);
					}
				}
			}
			// loss = Loss(lambda);

			double end = get_time();
			// cout << "Time per epoch: " << end-start << endl;

			loss = RMSE();
			cout << "Loss " << e << ": " << loss << endl;
		}
	}

	void OptimizeRound(float stepSize, float lambda, uint32_t numEpochs) {

		if (m_LBTiled.size() == 0) {
			cout << "m_LBTiled is empty" << endl;
			return;
		}

		float loss = RMSE();
		cout << "Initial Loss: " << loss << endl;

		for (uint32_t e = 0; e < numEpochs; e++) {

			double start = get_time();

			for (uint32_t tm = 0; tm < m_numTilesM; tm++) {
				for (uint32_t tu = 0; tu < m_numTilesU; tu++) {

					vector<LabelB> LTile = m_LBTiled[tm*m_numTilesU+tu];
					float* M_tile_offset = m_M + tm*m_tileSize*m_numFeatures;
					float* U_tile_offset = m_U + tu*m_tileSize*m_numFeatures;
					uint32_t M_min = tm*m_tileSize;
					uint32_t U_min = tu*m_tileSize;

					for (uint32_t i = 0; i < LTile.size(); i++) {

						float* M_vector = M_tile_offset + (LTile[i].m_Mindex-M_min)*m_numFeatures;
						float* U_vector = U_tile_offset + (LTile[i].m_Uindex-U_min)*m_numFeatures;

						float dot = Dot(M_vector, U_vector, m_numFeatures);
						float error = dot - LTile[i].m_value;

						cout << LTile[i].m_Mindex << "\t" << LTile[i].m_Uindex << "\t" << " dot: " << dot << endl;
						cout << LTile[i].m_Mindex << "\t" << LTile[i].m_Uindex << "\t" << " value: " << LTile[i].m_value << endl;
						cout << LTile[i].m_Mindex << "\t" << LTile[i].m_Uindex << "\t" << " error: " << error << endl;

						for (uint32_t j = 0; j < m_numFeatures; j++) {
							float M_temp = M_vector[j];
							float U_temp = U_vector[j];
							M_vector[j] = M_temp - stepSize*(error*U_temp + lambda*M_temp);
							U_vector[j] = U_temp - stepSize*(error*M_temp + lambda*U_temp);
						}
					}
				}
			}

			double end = get_time();
			// cout << "Time per epoch: " << end-start << endl;

			loss = RMSE();
			cout << "Loss " << e << ": " << loss << endl;
		}
	}
};