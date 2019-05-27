#include "LRMF.h"

void LRMF::ReadNetflixData(char* pathToFile, int Mdim, int Udim) {
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

	uint32_t totalNumEntries = 0;

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
			if (Udim != -1 && temp[j].m_Uindex > Udim) {
				continue;
			}

			totalNumEntries++;
			tempV.push_back(temp[j]);
			// cout << j << " User: " << m_L[i][j].m_Uindex << ", Rating: " << m_L[i][j].m_value << endl;

			if ((uint32_t)(temp[j].m_Uindex+1) > m_Udim) {
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
	cout << "totalNumEntries: " << totalNumEntries << endl;

	m_M = (float*)aligned_alloc(64, m_Mdim*m_numFeatures*sizeof(float));
	m_U = (float*)aligned_alloc(64, m_Udim*m_numFeatures*sizeof(float));
	RandInitMU();
	m_LBTiled.clear();
}

void LRMF::GenerateSyntheticData(int Mdim, uint32_t Udim) {
	srand(3);

	deallocData();

	m_Mdim = Mdim;
	m_Udim = Udim;

	uint32_t totalNumEntries = 0;
	m_L.reserve(m_Mdim);
	for (uint32_t i = 0; i < m_Mdim; i++) {

		uint32_t numEntries = RandRange(m_Udim*0.2);
		// cout << "Mindex: " << i << endl;
		// cout << "numEntries: " << numEntries << endl;
		totalNumEntries += numEntries;

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
	cout << "totalNumEntries: " << totalNumEntries << endl;

	m_M = (float*)aligned_alloc(64, m_Mdim*m_numFeatures*sizeof(float));
	m_U = (float*)aligned_alloc(64, m_Udim*m_numFeatures*sizeof(float));
	RandInitMU();
	m_LBTiled.clear();
}

void LRMF::DivideLBIntoTiles(uint32_t tileSize) {
	srand(3);

	m_LBTiled.clear();

	m_tileSize = tileSize;
	m_numTilesM = m_Mdim/m_tileSize + (m_Mdim%m_tileSize > 0);
	m_numTilesU = m_Udim/m_tileSize + (m_Udim%m_tileSize > 0);

	cout << "m_numTilesM: " << m_numTilesM << endl;
	cout << "m_numTilesU: " << m_numTilesU << endl;

	m_LBTiled.resize(m_numTilesM*m_numTilesU);

	for (uint32_t i = 0; i < m_LB.size(); i++) {
		uint32_t tile_m = m_LB[i].m_Mindex/m_tileSize;
		uint32_t tile_u = m_LB[i].m_Uindex/m_tileSize;

		if (tile_m < m_numTilesM && tile_u < m_numTilesU) { // Do this check because we have rest
			m_LBTiled[tile_m*m_numTilesU + tile_u].push_back(m_LB[i]);
		}
	}

	for (uint32_t t = 0; t < m_numTilesM*m_numTilesU; t++) {
		random_shuffle(m_LBTiled[t].begin(), m_LBTiled[t].end());
	}

	deallocData();

	m_M = (float*)aligned_alloc(64, m_numTilesM*m_tileSize*m_numFeatures*sizeof(float));
	m_U = (float*)aligned_alloc(64, m_numTilesU*m_tileSize*m_numFeatures*sizeof(float));
}

float LRMF::Loss(float lambda) {
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

float LRMF::RMSE() {
	float loss = 0.0;
	for (uint32_t m = 0; m < m_Mdim; m++) {
		float temploss = 0.0;
		float* M_vector = m_M + m*m_numFeatures;
		for (uint32_t u = 0; u < m_L[m].size(); u++) {
			float* U_vector = m_U + m_L[m][u].m_Uindex*m_numFeatures;

			float dot = Dot(M_vector, U_vector, m_numFeatures);
			float error = (m_L[m][u].m_value - dot);
			temploss += error*error;

			if(isnan(temploss)) {
				cout << "temploss is nan" << endl;
				cout << "m: " << m << endl;
				cout << "u: " << m_L[m][u].m_Uindex << endl;
				cout << "v: " << m_L[m][u].m_value << endl;
				cout << "dot: " << dot << endl;
				cout << "error: " << error << endl;

				for (uint32_t j = 0; j < m_numFeatures; j++) {
					cout << "M_vector " << j << ": " << M_vector[j] << endl;
				}
				for (uint32_t j = 0; j < m_numFeatures; j++) {
					cout << "U_vector " << j << ": " << U_vector[j] << endl;
				}
				exit(1);
			}
		}

		loss += temploss;
	}
	loss /= m_LB.size();
	return sqrt(loss);
}

void LRMF::OptimizeLR(float stepSize, float lambda, uint32_t numEpochs) {

	vector<float> G(m_Mdim, 1);
	vector<float> H(m_Udim, 1);

	float loss = RMSE();
	cout << "Initial Loss: " << loss << endl;

	double total = 0.0;

	for (uint32_t e = 0; e < numEpochs; e++) {
		double start = get_time();

		for (uint32_t m = 0; m < m_Mdim; m++) {
			float* M_vector = m_M + m*m_numFeatures;

			for (uint32_t u = 0; u < m_L[m].size(); u++) {
				float* U_vector = m_U + m_L[m][u].m_Uindex*m_numFeatures;
				float dot = Dot(M_vector, U_vector, m_numFeatures);
				float error = dot - m_L[m][u].m_value;

				float G_ = 0;
				float H_ = 0;
				float stepSizeG = stepSize*1.0/sqrt(G[m]);
				float stepSizeH = stepSize*1.0/sqrt(H[u]);

				for (uint32_t j = 0; j < m_numFeatures; j++) {
					float M_temp = M_vector[j];
					float U_temp = U_vector[j];

					float gradientM = (error*U_temp + lambda*M_temp);
					float gradientU = (error*M_temp + lambda*U_temp);

					G_ += gradientM*gradientM;
					H_ += gradientU*gradientU;

					M_vector[j] = M_temp - stepSizeG*gradientM;
					U_vector[j] = U_temp - stepSizeH*gradientU;
				}

				G[m] += G_/m_numFeatures;
				H[u] += H_/m_numFeatures;
			}
		}

		double end = get_time();
		total += (end-start);

		loss = RMSE();
		cout << "Loss " << e << ": " << loss << endl;
	}

	cout << "m_LB.size(): " << m_LB.size() << endl;
	cout << "GetDataSize()/1e9: " << GetDataSize()/1e9 << endl;
	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*GetDataSize())/total/1e9 << " GB/s" << endl;
}

void LRMF::OptimizeNaive(float stepSize, float lambda, uint32_t numEpochs) {
	float loss = RMSE();
	cout << "Initial Loss: " << loss << endl;

	double total = 0.0;

	random_shuffle(m_LB.begin(), m_LB.end());

	for (uint32_t e = 0; e < numEpochs; e++) {
		double start = get_time();

		for (uint32_t i = 0; i < m_LB.size(); i++ ) {
			float* M_vector = m_M + m_LB[i].m_Mindex*m_numFeatures;
			float* U_vector = m_U + m_LB[i].m_Uindex*m_numFeatures;
			float dot = Dot(M_vector, U_vector, m_numFeatures);
			float error = dot - m_LB[i].m_value;

			for (uint32_t j = 0; j < m_numFeatures; j++) {
				float M_temp = M_vector[j];
				float U_temp = U_vector[j];
				M_vector[j] = M_temp - stepSize*(error*U_temp + lambda*M_temp);
				U_vector[j] = U_temp - stepSize*(error*M_temp + lambda*U_temp);
			}
		}

		double end = get_time();
		total += (end-start);

		loss = RMSE();
		cout << "Loss " << e << ": " << loss << endl;
	}

	cout << "m_LB.size(): " << m_LB.size() << endl;
	cout << "GetDataSize()/1e9: " << GetDataSize()/1e9 << endl;
	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*GetDataSize())/total/1e9 << " GB/s" << endl;
}

void LRMF::Optimize(float stepSize, float lambda, uint32_t numEpochs) {
	float loss = RMSE();
	cout << "Initial Loss: " << loss << endl;

	double total = 0.0;

	for (uint32_t e = 0; e < numEpochs; e++) {
		double start = get_time();

		for (uint32_t m = 0; m < m_Mdim; m++) {
			float* M_vector = m_M + m*m_numFeatures;

			for (uint32_t u = 0; u < m_L[m].size(); u++) {
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

		double end = get_time();
		total += (end-start);

		loss = RMSE();
		cout << "Loss " << e << ": " << loss << endl;
	}

	cout << "m_LB.size(): " << m_LB.size() << endl;
	cout << "GetDataSize()/1e9: " << GetDataSize()/1e9 << endl;
	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*GetDataSize())/total/1e9 << " GB/s" << endl;
}

void LRMF::OptimizeRound(float stepSize, float lambda, uint32_t numEpochs) {

	if (m_LBTiled.size() == 0) {
		cout << "m_LBTiled is empty" << endl;
		return;
	}

	float loss = RMSE();
	cout << "Initial Loss: " << loss << endl;

	double total = 0.0;

	// random_shuffle(m_LBTiled.begin(), m_LBTiled.end());

	for (uint32_t e = 0; e < numEpochs; e++) {

		double start = get_time();

		for (uint32_t tm = 0; tm < m_numTilesM; tm++) {
			for (uint32_t tu = 0; tu < m_numTilesU; tu++) {
				// uint32_t temp_tu = m_numTilesU*((float)rand()/(float)RAND_MAX);
				// uint32_t temp_tu = m_numTilesU-tu-1;
				uint32_t temp_tu = tu;

				vector<LabelB> LTile = m_LBTiled[tm*m_numTilesU+temp_tu];
				float* M_tile_offset = m_M + tm*m_tileSize*m_numFeatures;
				float* U_tile_offset = m_U + temp_tu*m_tileSize*m_numFeatures;
				uint32_t M_min = tm*m_tileSize;
				uint32_t U_min = temp_tu*m_tileSize;

				// cout << "------------------------------------------------------------------------------------------" << endl;
				// cout << tm << " " << temp_tu << " LTile.size(): " << LTile.size() << endl;

				for (uint32_t i = 0; i < LTile.size(); i++) {

					// cout << "-----------------------" << endl;
					// cout << "m_Mindex: " << LTile[i].m_Mindex << endl;
					// cout << "m_Uindex: " << LTile[i].m_Uindex << endl;
					// cout << "m_value: " << LTile[i].m_value << endl;

					float* M_vector = M_tile_offset + (LTile[i].m_Mindex-M_min)*m_numFeatures;
					float* U_vector = U_tile_offset + (LTile[i].m_Uindex-U_min)*m_numFeatures;

					// float* M_vector = m_M + LTile[i].m_Mindex*m_numFeatures;
					// float* U_vector = m_U + LTile[i].m_Uindex*m_numFeatures;

					float dot = Dot(M_vector, U_vector, m_numFeatures);
					float error = dot - LTile[i].m_value;

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
		total += (end-start);

		loss = RMSE();
		cout << "Loss " << e << ": " << loss << endl;
	}

	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*GetDataSize())/total/1e9 << " GB/s" << endl;
}

void LRMF::OptimizeRoundStale(float stepSize, float lambda, uint32_t numEpochs) {

	if (m_LBTiled.size() == 0) {
		cout << "m_LBTiled is empty" << endl;
		return;
	}

	float loss = RMSE();
	cout << "Initial Loss: " << loss << endl;

	float* M_tile_new = (float*)malloc(m_tileSize*m_numFeatures*sizeof(float));
	float* U_tile_new = (float*)malloc(m_tileSize*m_numFeatures*sizeof(float));

	double total = 0.0;

	for (uint32_t e = 0; e < numEpochs; e++) {

		double start = get_time();

		for (uint32_t tm = 0; tm < m_numTilesM; tm++) {
			for (uint32_t tu = 0; tu < m_numTilesU; tu++) {

				// uint32_t temp_tu = m_numTilesU*((float)rand()/(float)RAND_MAX);
				uint32_t temp_tu = tu;
				// uint32_t temp_tu = m_numTilesU - tu - 1;

				vector<LabelB> LTile = m_LBTiled[tm*m_numTilesU+temp_tu];
				float* M_tile_offset = m_M + tm*m_tileSize*m_numFeatures;
				float* U_tile_offset = m_U + temp_tu*m_tileSize*m_numFeatures;
				uint32_t M_min = tm*m_tileSize;
				uint32_t U_min = temp_tu*m_tileSize;

				memcpy(M_tile_new, M_tile_offset, m_tileSize*m_numFeatures*sizeof(float));
				memcpy(U_tile_new, U_tile_offset, m_tileSize*m_numFeatures*sizeof(float));

				// cout << "------------------------------------------------------------------------------------------" << endl;
				// cout << tm << " " << temp_tu << " LTile.size(): " << LTile.size() << endl;

				for (uint32_t i = 0; i < LTile.size(); i++) {

					float* M_vector = M_tile_offset + (LTile[i].m_Mindex-M_min)*m_numFeatures;
					float* U_vector = U_tile_offset + (LTile[i].m_Uindex-U_min)*m_numFeatures;

					// for (uint32_t j = 0; j < m_numFeatures; j++) {
					// 	cout << "M_vector[" << j << "]: " << M_vector[j] << endl;
					// }
					// for (uint32_t j = 0; j < m_numFeatures; j++) {
					// 	cout << "U_vector[" << j << "]: " << U_vector[j] << endl;
					// }

					float dot = Dot(M_vector, U_vector, m_numFeatures);
					float error = dot - LTile[i].m_value;

					// cout << LTile[i].m_Mindex << "\t" << LTile[i].m_Uindex << "\t" << " dot: " << dot << endl;
					// cout << LTile[i].m_Mindex << "\t" << LTile[i].m_Uindex << "\t" << " value: " << LTile[i].m_value << endl;
					// cout << LTile[i].m_Mindex << "\t" << LTile[i].m_Uindex << "\t" << " error: " << error << endl;

					float* M_vector_new = M_tile_new + (LTile[i].m_Mindex-M_min)*m_numFeatures;
					float* U_vector_new = U_tile_new + (LTile[i].m_Uindex-U_min)*m_numFeatures;

					for (uint32_t j = 0; j < m_numFeatures; j++) {
						// cout << "error*U_vector[" << j << "]: " << stepSize*error*U_vector[j] << endl;
						M_vector_new[j] = M_vector_new[j] - stepSize*(error*U_vector[j] + lambda*M_vector[j]);
						U_vector_new[j] = U_vector_new[j] - stepSize*(error*M_vector[j] + lambda*U_vector[j]);
					}
				}

				memcpy(M_tile_offset, M_tile_new, m_tileSize*m_numFeatures*sizeof(float));
				memcpy(U_tile_offset, U_tile_new, m_tileSize*m_numFeatures*sizeof(float));
			}
		}

		double end = get_time();
		total += (end-start);

		loss = RMSE();
		cout << loss << endl;
	}

	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*GetDataSize())/total/1e9 << " GB/s" << endl;

	free(M_tile_new);
	free(U_tile_new);
}

void LRMF::DivideWorkByThread(
	uint32_t numThreads,
	vector<uint32_t>& MtileToStart,
	vector<uint32_t>& numTilesM,
	vector<uint32_t>& UtileToStart,
	vector<uint32_t>& numTilesU)
{
	vector<uint32_t> MtilesPermutation(numThreads*numThreads);
	vector<uint32_t> UtilesPermutation(numThreads*numThreads);
	for (uint32_t i = 0; i < numThreads; i++) {
		for (uint32_t j = 0; j < numThreads; j++) {
			MtilesPermutation[i*numThreads+j] = j;
			UtilesPermutation[i*numThreads+j] = (i+j)%numThreads;
		}
	}

	MtileToStart.resize(numThreads*numThreads);
	numTilesM.resize(numThreads*numThreads);
	UtileToStart.resize(numThreads*numThreads);
	numTilesU.resize(numThreads*numThreads);
	uint32_t MtilesPerInstance = GetNumTilesM()/numThreads;
	uint32_t UtilesPerInstance = GetNumTilesU()/numThreads;

	for (uint32_t i = 0; i < numThreads; i++) {
		for (uint32_t j = 0; j < numThreads; j++) {
			MtileToStart[i*numThreads+j] = MtilesPermutation[i*numThreads+j]*MtilesPerInstance;
			if (MtilesPermutation[i*numThreads+j] == numThreads-1)
				numTilesM[i*numThreads+j] = GetNumTilesM() - MtileToStart[i*numThreads+j];
			else
				numTilesM[i*numThreads+j] = MtilesPerInstance;

			UtileToStart[i*numThreads+j] = UtilesPermutation[i*numThreads+j]*UtilesPerInstance;
			if (UtilesPermutation[i*numThreads+j] == numThreads-1)
				numTilesU[i*numThreads+j] = GetNumTilesU() - UtileToStart[i*numThreads+j];
			else
				numTilesU[i*numThreads+j] = UtilesPerInstance;

			// cout << "MtileToStart " << i << " " << j << ": " << MtileToStart[i*numThreads+j] << endl;
			cout << "numTilesM " << i << " " << j << ": " << numTilesM[i*numThreads+j] << endl;
			// cout << "UtileToStart " << i << " " << j << ": " << UtileToStart[i*numThreads+j] << endl;
			cout << "numTilesU " << i << " " << j << ": " << numTilesU[i*numThreads+j] << endl;
		}
	}
}

void LRMF::UpdateTile(
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
			float lambda)
{
	for (uint32_t tm = MtileToStart; tm < MtileToStart+numTilesM; tm++) {
		for (uint32_t tu = UtileToStart; tu < UtileToStart+numTilesU; tu++) {
			// uint32_t temp_tu = UtileToStart+numTilesU - tu - 1;
			uint32_t temp_tu = tu;

			vector<LabelB> LTile = (*m_LBTiled)[tm*totalNumTilesU+temp_tu];
			float* M_tile_offset = m_M + tm*m_tileSize*m_numFeatures;
			float* U_tile_offset = m_U + temp_tu*m_tileSize*m_numFeatures;
			uint32_t M_min = tm*m_tileSize;
			uint32_t U_min = temp_tu*m_tileSize;

			for (uint32_t i = 0; i < LTile.size(); i++) {

				float* M_vector = M_tile_offset + (LTile[i].m_Mindex-M_min)*m_numFeatures;
				float* U_vector = U_tile_offset + (LTile[i].m_Uindex-U_min)*m_numFeatures;

				float dot = Dot(M_vector, U_vector, m_numFeatures);
				float error = dot - LTile[i].m_value;

				for (uint32_t j = 0; j < m_numFeatures; j++) {
					float M_temp = M_vector[j];
					float U_temp = U_vector[j];
					M_vector[j] = M_temp - stepSize*(error*U_temp + lambda*M_temp);
					U_vector[j] = U_temp - stepSize*(error*M_temp + lambda*U_temp);
				}
			}
		}
	}
}

void LRMF::OptimizeRoundMulti(float stepSize, float lambda, uint32_t numEpochs, uint32_t numThreads) {

	if (m_LBTiled.size() == 0) {
		cout << "m_LBTiled is empty" << endl;
		return;
	}

	float loss = RMSE();
	cout << "Initial Loss: " << loss << endl;

	vector<uint32_t> MtileToStart;
	vector<uint32_t> numTilesM;
	vector<uint32_t> UtileToStart;
	vector<uint32_t> numTilesU;
	DivideWorkByThread(numThreads, MtileToStart, numTilesM, UtileToStart, numTilesU);

	double total = 0.0;
	vector<thread*> threads(numThreads);
	for (uint32_t e = 0; e < numEpochs; e++) {
		double start = get_time();

		for (uint32_t i = 0; i < numThreads; i++) {
			for (uint32_t j = 0; j < numThreads; j++) {
				threads[j] =
					new thread(
						UpdateTile,
						j,
						m_M,
						m_U,
						&m_LBTiled,
						MtileToStart[i*numThreads+j],
						numTilesM[i*numThreads+j],
						UtileToStart[i*numThreads+j],
						numTilesU[i*numThreads+j],
						m_numTilesU,
						m_numFeatures,
						m_tileSize,
						stepSize,
						lambda);
			}
			for (uint32_t j = 0; j < numThreads; j++) {
				threads[j]->join();
				delete threads[j];
			}
		}

		double end = get_time();
		total += (end-start);

		loss = RMSE();
		cout << loss << endl;
	}

	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*GetDataSize())/total/1e9 << " GB/s" << endl;
}