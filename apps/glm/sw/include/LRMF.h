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

using namespace std;

struct Label {
	int m_Uindex;
	int m_value;
};

class LRMF {
private:

	uint32_t m_numFeatures;
	uint32_t m_Mdim;
	uint32_t m_Udim;

	float* m_M; // m_Mdim x m_numFeatures
	float* m_U; // m_Udim x m_numFeatures
	vector< vector<Label> > m_L;

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

	void RandInit(float* base, uint32_t lenght) {
		for (uint32_t i = 0; i < lenght; i++) {
			base[i] = (float)rand()/(float)RAND_MAX;
		}
	}

public:
	LRMF(uint32_t numFeatures) {
		srand(7);
		m_numFeatures = numFeatures;
		m_Mdim = 0;
		m_Udim = 0;
		m_M = nullptr;
		m_U = nullptr;
	}

	~LRMF() {
		deallocData();
	}

	void ReadNetflixData(char* pathToFile) {
		FILE* f = fopen(pathToFile, "r");
		if (f == NULL) {
			cout << "Can't find files at pathToFile" << endl;
			return;
		}

		deallocData();

		size_t readsize;

		readsize = fread(&m_Mdim, sizeof(uint32_t), 1, f);
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
			}
			free(temp);

			m_L.push_back(tempV);
		}
		cout << "m_Udim: " << m_Udim << endl;

		m_M = (float*)aligned_alloc(64, m_Mdim*m_numFeatures*sizeof(float));
		RandInit(m_M, m_Mdim*m_numFeatures);
		m_U = (float*)aligned_alloc(64, m_Udim*m_numFeatures*sizeof(float));
		RandInit(m_U, m_Mdim*m_numFeatures);
	}

	float Dot(float* vector1, float* vector2, uint32_t numFeatures) {
		float dot = 0.0;
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			dot += vector1[j]*vector2[j];
		}
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
			for (uint32_t u = 0; u < m_L[m].size(); u++) {
				float* M_vector = m_M + m*m_numFeatures;
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
			for (uint32_t m = 0; m < m_Mdim; m++) {
				// cout << "m: " << m << endl;
				// cout << "m_L[m].size(): " << m_L[m].size() << endl;
				for (uint32_t u = 0; u < m_L[m].size(); u++) {

					// cout << "m_L[m][u].m_Uindex: " << m_L[m][u].m_Uindex << endl;

					float* M_vector = m_M + m*m_numFeatures;
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
			loss = RMSE();
			cout << "Loss " << e << ": " << loss << endl;
		}

	}
};