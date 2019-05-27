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

#include "FPGA_ColumnML.h"

void FPGA_ColumnML::UseCreatedMemoryLayout(FPGA_ColumnML* baseObj) {
	if (m_inputHandle != NULL) {
		cout << "m_inputHandle of FPGA_ColumnML object created as clone is not NULL" << endl;
		return;
	}

	if (baseObj->GetBank() == GetBank()) {
		m_base = baseObj->GetBase();
		m_inputHandle = baseObj->m_inputHandle;
		m_inputSizeInCL = baseObj->m_inputSizeInCL;
	}
	else {
		m_inputSizeInCL = baseObj->m_inputSizeInCL;
		m_ifpga->Realloc(m_inputHandle, m_inputSizeInCL*64);
		m_base = iFPGA::CastToFloat(m_inputHandle);
		memcpy((void*)m_base, (void*)baseObj->GetBase(), 64*m_inputSizeInCL);
	}

	m_modelChunk = baseObj->m_modelChunk;
	m_labelsChunk = baseObj->m_labelsChunk;
	m_samplesChunk = baseObj->m_samplesChunk;
	m_residualChunk = baseObj->m_residualChunk;
	m_accesspropsChunk = baseObj->m_accesspropsChunk;

	m_numSamples = baseObj->m_numSamples;
	m_numFeatures = baseObj->m_numFeatures;
	m_numSamplesInCL = baseObj->m_numSamplesInCL;
	m_numFeaturesInCL = baseObj->m_numFeaturesInCL;
	m_partitionSize = baseObj->m_partitionSize;
	m_partitionSizeInCL = baseObj->m_partitionSizeInCL;
	m_alignedNumSamples = baseObj->m_alignedNumSamples;
	m_alignedNumFeatures = baseObj->m_alignedNumFeatures;
	m_alignedPartitionSize = baseObj->m_alignedPartitionSize;
	m_numPartitions = baseObj->m_numPartitions;
	m_rest = baseObj->m_rest;
	m_restInCL = baseObj->m_restInCL;
	m_numEpochs = baseObj->m_numEpochs;
}

uint32_t FPGA_ColumnML::CreateMemoryLayout(MemoryFormat format, uint32_t partitionSize, uint32_t numEpochs, bool useOnehotLabels) {
	m_currentMemoryFormat = format;

	m_numSamples = m_cstore->m_numSamples;
	m_numFeatures = m_cstore->m_numFeatures;
	m_numSamplesInCL = (m_numSamples >> 4) + ((m_numSamples&0xF) > 0);
	m_numFeaturesInCL = (m_numFeatures >> 4) + ((m_numFeatures&0xF) > 0);
	m_partitionSize = (partitionSize > m_numSamples) ? m_numSamples : partitionSize;
	m_partitionSizeInCL = (m_partitionSize >> 4) + ((m_partitionSize & 0xF) > 0);
	m_alignedNumSamples = m_numSamplesInCL*16;
	m_alignedNumFeatures = m_numFeaturesInCL*16;
	m_alignedPartitionSize = m_partitionSizeInCL*16;
	m_numPartitions = m_numSamples/m_partitionSize;
	m_rest = m_numSamples % m_partitionSize;
	m_restInCL = (m_rest >> 4) + ((m_rest&0xF) > 0);
	m_numEpochs = numEpochs;

	cout << "m_numSamplesInCL: " << m_numSamplesInCL << endl;
	cout << "m_numFeaturesInCL: " << m_numFeaturesInCL << endl;
	cout << "m_partitionSize: " << m_partitionSize << endl;
	cout << "m_partitionSizeInCL: " << m_partitionSizeInCL << endl;
	cout << "m_alignedNumSamples: " << m_alignedNumSamples << endl;
	cout << "m_alignedNumFeatures: " << m_alignedNumFeatures << endl;
	cout << "m_alignedPartitionSize: " << m_alignedPartitionSize << endl;
	cout << "m_numPartitions: " << m_numPartitions << endl;
	cout << "m_rest: " << m_rest << endl;
	cout << "m_restInCL: " << m_restInCL << endl;
	cout << "m_numEpochs: " << m_numEpochs << endl;

	uint32_t countCL = 0;

	if (format == RowStore) {
		// Model
		m_modelChunk.m_offsetInCL = countCL;
		countCL += m_numFeaturesInCL;
		m_modelChunk.m_lengthInCL = countCL - m_modelChunk.m_offsetInCL;

		// Labels
		m_labelsChunk.m_offsetInCL = countCL;
		countCL += useOnehotLabels ? m_cstore->m_onehotLabels.size()*m_numSamplesInCL : m_numSamplesInCL;
		m_labelsChunk.m_lengthInCL = countCL - m_labelsChunk.m_offsetInCL;

		// Samples
		m_samplesChunk.m_offsetInCL = countCL;
		countCL += m_numSamples*m_numFeaturesInCL;
		m_samplesChunk.m_lengthInCL = countCL - m_samplesChunk.m_offsetInCL;

		m_ifpga->Realloc(m_inputHandle, countCL*64);
		m_base = iFPGA::CastToFloat(m_inputHandle);
		memset((void*)m_base, 0, 16*countCL*sizeof(float));

		m_model = m_base + m_modelChunk.m_offsetInCL*16;
		m_labels = m_base + m_labelsChunk.m_offsetInCL*16;
		m_samples = m_base + m_samplesChunk.m_offsetInCL*16;

		for (uint32_t j = 0; j < m_numFeatures; j++) {
			m_model[j] = 0;
		}
		for (uint32_t i = 0; i < m_numSamples; i++) {
			
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				m_samples[i*m_alignedNumFeatures + j] = m_cstore->m_samples[j][i];
			}
		}
		if (useOnehotLabels) {
			for (uint32_t c = 0; c < m_cstore->m_onehotLabels.size(); c++) {
				for (uint32_t i = 0; i < m_numSamples; i++) {
					m_labels[c*m_alignedNumSamples + i] = m_cstore->m_onehotLabels[c][i];
				}
			}
		}
		else {
			for (uint32_t i = 0; i < m_numSamples; i++) {
				m_labels[i] = m_cstore->m_labels[i];
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
		m_columnsChunks.resize(m_numFeatures*m_numPartitions);
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			for (uint32_t p = 0; p < m_numPartitions; p++) {
				m_columnsChunks[j*m_numPartitions+p].m_offsetInCL = countCL;
				countCL += m_partitionSizeInCL;
				m_columnsChunks[j*m_numPartitions+p].m_lengthInCL = countCL - m_columnsChunks[j*m_numPartitions+p].m_offsetInCL;
#ifdef DEBUG_COPY
				cout << "m_columnsChunks[" << j << "," << p << "].m_offsetInCL: " << m_columnsChunks[j*m_numPartitions+p].m_offsetInCL << endl;
				cout << "m_columnsChunks[" << j << "," << p << "].m_lengthInCL: " << m_columnsChunks[j*m_numPartitions+p].m_lengthInCL << endl;
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
		m_columns.resize(m_numFeatures);
		for (uint32_t j = 0; j < m_numFeatures; j++) {
			m_columns[j] = m_base + m_columnsChunks[j*m_numPartitions].m_offsetInCL*16;
		}

		for (uint32_t i = 0; i < m_numSamples; i++) {
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
			for (uint32_t j = 0; j < m_numFeatures; j++) {
				m_accessprops[p*m_alignedNumFeatures*2 + 2*j] = m_columnsChunks[j*m_numPartitions+p].m_offsetInCL;
				m_accessprops[p*m_alignedNumFeatures*2 + 2*j+1] = m_columnsChunks[j*m_numPartitions+p].m_lengthInCL;
				for (uint32_t i = 0; i < m_partitionSize; i++) {
					m_columns[j][p*m_alignedPartitionSize + i] = m_cstore->m_samples[j][p*m_alignedPartitionSize + i];
				}
			}
		}
	}
	m_inputSizeInCL = countCL;

	return countCL;
}

bool FPGA_ColumnML::fSGD(
	ModelType type,
	uint32_t numEpochs,
	float stepSize,
	float lambda,
	uint32_t whichClass)
{
	if (m_base == nullptr) {
		cout << "m_base is nullptr!" << endl;
		return false;
	}

	uint32_t modelOffsetInBRAM = 0;
	uint32_t labelOffsetInBRAM = 0;

	float scaledLambda = stepSize*lambda;

	// *************************************************************************
	//
	//   START Program
	//
	// *************************************************************************
	uint32_t pc = 0;

	// Load model
	localaccess_t loadModelWrite(modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].ResetIndex(2);
	pc++;

	uint32_t beginEpoch = pc;

	localaccess_t modelCopy(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Copy(modelCopy, modelCopy);
	pc++;

	// Load labels in partition
	localaccess_t loadLabelsWrite(labelOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL + whichClass*m_numSamplesInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
	pc++;

	m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	localaccess_t loadSamplesWrite(FIFO, 0, m_numFeaturesInCL);
	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
	m_inst[pc].MakeNonBlocking();
	pc++;

	localaccess_t dotLeftRead(FIFO, 0, m_numFeaturesInCL);
	localaccess_t dotRightRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	localaccess_t dotWrite(FIFO, 1);
	m_inst[pc].Dot(m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, (type == logreg));
	pc++;

	// Start---Innermost loop
	localaccess_t labelsRead(BRAM, 0, 1);
	localaccess_t modifyWrite(FIFO, 1);
	m_inst[pc].Modify(type, 0, stepSize, 0, labelsRead, modifyWrite);
	m_inst[pc].MakeNonBlocking();
	uint32_t pcModify = pc;
	pc++;

	localaccess_t updateSamplesRead(FIFO, 0, m_numFeaturesInCL);
	localaccess_t updateModelRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	localaccess_t updateModelWrite(FIFOBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Update(m_numFeaturesInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	m_inst[pc].MakeNonBlocking();
	m_inst[pc].IncrementIndex(0);
	pc++;

	localaccess_t copyModelRead(FIFO, m_numFeaturesInCL);
	m_inst[pc].L2Reg(m_numFeaturesInCL, modelCopy, copyModelRead, copyModelRead, modelCopy, scaledLambda);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
	m_inst[pc].MakeNonBlocking();
	pc++;

	dotRightRead.Set(FIFO, m_numFeaturesInCL);
	m_inst[pc].Dot(m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, (type == logreg));
	m_inst[pc].Jump(0, m_partitionSize-1, pcModify, pc+1);
	pc++;
	// End---Innermost loop

	m_inst[pc].Modify(type, 0, stepSize, 0, labelsRead, modifyWrite);
	m_inst[pc].MakeNonBlocking();
	pc++;

	updateModelWrite.Set(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Update(m_numFeaturesInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	pc++;

	m_inst[pc].L2Reg(m_numFeaturesInCL, modelCopy, modelCopy, modelCopy, modelCopy, scaledLambda);
	m_inst[pc].Jump(1, m_numPartitions-1, beginEpoch, pc+1);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].IncrementIndex(1);
	if (m_useContextSwitch) {
		m_inst[pc].EnableContextSwitch();
	}
	pc++;

	if ( m_rest > 1 ) {
		m_inst[pc].Copy(modelCopy, modelCopy);
		pc++;

		loadLabelsWrite.Set(labelOffsetInBRAM, m_restInCL);
		m_inst[pc].Load(m_labelsChunk.m_offsetInCL + whichClass*m_numSamplesInCL, m_restInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
		pc++;

		m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
		m_inst[pc].MakeNonBlocking();
		pc++;

		dotRightRead.Set(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
		m_inst[pc].Dot(m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, (type == logreg));
		pc++;

		// Start---Innermost loop
		m_inst[pc].Modify(type, 0, stepSize, 0, labelsRead, modifyWrite);
		m_inst[pc].MakeNonBlocking();
		uint32_t pcRestSamples = pc;
		pc++;

		updateModelWrite.Set(FIFOBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
		m_inst[pc].Update(m_numFeaturesInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
		m_inst[pc].MakeNonBlocking();
		m_inst[pc].IncrementIndex(0);
		pc++;

		m_inst[pc].L2Reg(m_numFeaturesInCL, modelCopy, copyModelRead, copyModelRead, modelCopy, scaledLambda);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
		m_inst[pc].MakeNonBlocking();
		pc++;

		dotRightRead.Set(FIFO, m_numFeaturesInCL);
		m_inst[pc].Dot(m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, (type == logreg));
		m_inst[pc].Jump(0, m_rest-1, pcRestSamples, pc+1);
		pc++;
		// End---Innermost loop

		m_inst[pc].Modify(type, 0, stepSize, 0, labelsRead, modifyWrite);
		m_inst[pc].MakeNonBlocking();
		pc++;

		updateModelWrite.Set(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
		m_inst[pc].Update(m_numFeaturesInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
		pc++;

		m_inst[pc].L2Reg(m_numFeaturesInCL, modelCopy, modelCopy, modelCopy, modelCopy, scaledLambda);
		pc++;
	}

	// WriteBack
	localaccess_t writebackModelRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
		0, 0, m_numFeaturesInCL,
		true, writebackModelRead, Instruction::WRITEBACK_MODEL_CHANNEL);
	pc++;

	m_inst[pc].Jump(2, numEpochs-1, beginEpoch, 0xFFFFFFFF);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].IncrementIndex(2);
	pc++;

	// Context Store Instructions
	uint32_t pcContextStore = pc;
	m_inst[pc].WriteBack(true, m_modelChunk.m_offsetInCL, m_numFeaturesInCL,
		0, 0, 0,
		true, writebackModelRead, Instruction::WRITEBACK_MODEL_CHANNEL);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF0, 0xFFFFFFF0);
	pc++;

	// Context Load Instructions
	uint32_t pcContextLoad = pc;
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF1, 0xFFFFFFF1);
	pc++;

	// *************************************************************************
	//
	//   END Program
	//
	// *************************************************************************
	m_outputSizeInCL = (numEpochs*m_numFeaturesInCL+1);
	m_ifpga->Realloc(m_outputHandle, m_outputSizeInCL*64);

	m_numInstructions = pc;
	WriteProgramMemory(pcContextStore, pcContextLoad);

	return true;
}

bool FPGA_ColumnML::fSGD_minibatch(
	ModelType type,
	uint32_t numEpochs,
	uint32_t minibatchSize, 
	float stepSize,
	float lambda,
	uint32_t whichClass)
{
	if (m_base == nullptr) {
		cout << "m_base is nullptr!" << endl;
		return false;
	}
	if (m_partitionSize%minibatchSize > 0) {
		cout << "m_partitionSize%minibatchSize = 0 must hold!" << endl;
		return false;
	}
	if (m_partitionSize/minibatchSize == 1) {
		cout << "m_partitionSize/minibatchSize > 1 must hold!" << endl;
		return false;
	}

	float scaledStepSize = stepSize/minibatchSize;
	float scaledLambda = stepSize*lambda;

	uint32_t modelOffsetInBRAM = 0;
	uint32_t labelOffsetInBRAM = 0;

	// *************************************************************************
	//
	//   START Program
	//
	// *************************************************************************
	uint32_t pc = 0;

	// Load model
	localaccess_t loadModelWrite(modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].ResetIndex(2);
	pc++;

	localaccess_t modelCopy(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Copy(modelCopy, modelCopy);
	pc++;

	// Load labels in partition
	localaccess_t loadLabelsWrite(labelOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL + whichClass*m_numSamplesInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
	uint32_t pcLabels = pc;
	pc++;

	m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	// Start---Innermost loop
	localaccess_t loadSamplesWrite(FIFO, 0, minibatchSize*m_numFeaturesInCL);
	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, minibatchSize*m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
	uint32_t pcSamples = pc;
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].L2Reg(m_numFeaturesInCL, modelCopy, modelCopy, modelCopy, modelCopy, scaledLambda);
	pc++;

	localaccess_t dotLeftRead(FIFO, 0, m_numFeaturesInCL);
	localaccess_t dotRightRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	localaccess_t dotWrite(FIFO, 1);
	m_inst[pc].Dot(minibatchSize, m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, (type == logreg));
	m_inst[pc].MakeNonBlocking();
	pc++;

	localaccess_t labelsRead(BRAM, 0, 1);
	localaccess_t modifyWrite(FIFO, 1);
	m_inst[pc].Modify(minibatchSize, type, 0, scaledStepSize, 0, labelsRead, modifyWrite);
	m_inst[pc].MakeNonBlocking();
	pc++;

	localaccess_t updateSamplesRead(FIFO, m_numFeaturesInCL);
	localaccess_t updateModelRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	localaccess_t updateModelWrite(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Update(minibatchSize, m_numFeaturesInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	m_inst[pc].IncrementIndex(0, minibatchSize);
	m_inst[pc].Jump(0, m_partitionSize-minibatchSize, pcSamples, pc+1);
	pc++;
	// End---Innermost loop

	m_inst[pc].Jump(1, m_numPartitions-1, pcLabels, pc+1);
	if (m_useContextSwitch) {
		m_inst[pc].EnableContextSwitch();
	}
	m_inst[pc].ResetIndex(0);
	m_inst[pc].IncrementIndex(1);
	pc++;

	if ( m_rest > 0 ) {
		loadLabelsWrite.Set(labelOffsetInBRAM, m_restInCL);
		m_inst[pc].Load(m_labelsChunk.m_offsetInCL + whichClass*m_numSamplesInCL, m_restInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
		pc++;

		// Start---Innermost loop
		loadSamplesWrite.Set(FIFO, 0, m_rest*m_numFeaturesInCL);
		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
		uint32_t pcRestSamples = pc;
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].L2Reg(m_numFeaturesInCL, modelCopy, modelCopy, modelCopy, modelCopy, scaledLambda);
		pc++;

		m_inst[pc].Dot(m_rest, m_numFeaturesInCL, dotLeftRead, dotRightRead, dotWrite, (type == logreg));
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Modify(m_rest, type, 0, scaledStepSize, lambda, labelsRead, modifyWrite);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Update(m_rest, m_numFeaturesInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
		pc++;
		// End---Innermost loop
	}

	// WriteBack
	localaccess_t writebackModelRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
		0, 0, m_numFeaturesInCL,
		true, writebackModelRead, Instruction::WRITEBACK_MODEL_CHANNEL);
	pc++;

	m_inst[pc].Jump(2, numEpochs-1, pcLabels, 0xFFFFFFFF);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].IncrementIndex(2);
	pc++;

	// Context Store Instructions
	uint32_t pcContextStore = pc;
	m_inst[pc].WriteBack(true, m_modelChunk.m_offsetInCL, m_numFeaturesInCL,
		0, 0, 0,
		true, writebackModelRead, Instruction::WRITEBACK_MODEL_CHANNEL);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF0, 0xFFFFFFF0);
	pc++;

	// Context Load Instructions
	uint32_t pcContextLoad = pc;
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF1, 0xFFFFFFF1);
	pc++;

	// *************************************************************************
	//
	//   END Program
	//
	// *************************************************************************
	m_outputSizeInCL = (numEpochs*m_numFeaturesInCL+1);
	m_ifpga->Realloc(m_outputHandle, m_outputSizeInCL*64);

	m_numInstructions = pc;
	WriteProgramMemory(pcContextStore, pcContextLoad);

	return true;
}

bool FPGA_ColumnML::fSCD(
	uint32_t partitionToStart,
	uint32_t numPartitions,
	ModelType type, 
	uint32_t numEpochs,
	float stepSize, 
	float lambda)
{
	if (m_base == nullptr) {
		cout << "m_base is nullptr!" << endl;
		return false;
	}
	if (numEpochs != m_numEpochs) {
		cout << "numEpochs has to match the one used to create the memory layout" << endl;
		return false;
	}

	float scaledStepSize = stepSize/m_partitionSize;
	float scaledLambda = stepSize*lambda;

	uint32_t residualOffsetInBRAM = 0;
	uint32_t labelOffsetInBRAM = 0;
	uint32_t modelOffsetInBRAM = labelOffsetInBRAM + m_partitionSizeInCL;
	uint32_t accesspropsOffsetInBRAM = 0;

	// *************************************************************************
	//
	//   START Program
	//
	// *************************************************************************
	uint32_t pc = 0;

	m_inst[pc].SetIndex(1, partitionToStart);
	uint32_t pcEpochStart = pc;
	pc++;

	m_inst[pc].ResetIndex(0);
	uint32_t pcPartitionStart = pc;
	pc++;

	// Load residual
	localaccess_t loadResidualWrite(residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_residualChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadResidualWrite, Instruction::LOAD_REGION_MODEL_CHANNEL);
	pc++;

	// Load labels in partition
	localaccess_t loadLabelsWrite(labelOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
	pc++;

	// Load model
	localaccess_t loadModelWrite(modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_numFeaturesInCL, 0, m_numFeaturesInCL, m_numPartitions*m_numFeaturesInCL, loadModelWrite, Instruction::LOAD_REGION_LABELS_CHANNEL);
	pc++;

	// Load accessprops
	localaccess_t loadAccesspropsWrite(accesspropsOffsetInBRAM, 2*m_numFeaturesInCL);
	m_inst[pc].Load(m_accesspropsChunk.m_offsetInCL, 2*m_numFeaturesInCL, 0, 2*m_numFeaturesInCL, 0, loadAccesspropsWrite, Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL);
	pc++;

	// Load samples
	localaccess_t loadSamplesWrite(FIFO, 0, m_partitionSizeInCL);
	m_inst[pc].LocalLoad(0, 1, 0, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
	m_inst[pc].MakeNonBlocking();
	pc++;

	localaccess_t deltaLeftRead(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	localaccess_t deltaRightRead(BRAM, labelOffsetInBRAM, m_partitionSizeInCL);
	localaccess_t deltaWrite(FIFO, m_partitionSizeInCL);
	m_inst[pc].Delta(m_partitionSizeInCL, deltaLeftRead, deltaRightRead, deltaWrite, (type == logreg));
	m_inst[pc].MakeNonBlocking();
	pc++;

	localaccess_t dotLeftRead(FIFO, 0, m_partitionSizeInCL);
	localaccess_t dotRightRead(FIFO, 0, m_partitionSizeInCL);
	localaccess_t dotWrite(FIFO, m_partitionSizeInCL);
	m_inst[pc].Dot(m_partitionSizeInCL, dotLeftRead, dotRightRead, dotWrite, false);
	pc++;

	// Start---Innermost loop
	localaccess_t modelRead(BRAM, modelOffsetInBRAM, 1);
	localaccess_t modifyWrite(FIFO, 1);
	m_inst[pc].Modify(type, 1, scaledStepSize, scaledLambda, modelRead, modifyWrite);
	m_inst[pc].MakeNonBlocking();
	uint32_t pcModify = pc;
	pc++;

	localaccess_t updateSamplesRead(FIFO, 0, m_partitionSizeInCL);
	localaccess_t updateModelRead(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	localaccess_t updateModelWrite(FIFOBRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Update(m_partitionSizeInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	m_inst[pc].MakeNonBlocking();
	m_inst[pc].IncrementIndex(0);
	pc++;

	m_inst[pc].LocalLoad(0, 1, 0, 0, loadSamplesWrite, Instruction::LOAD_REGION_INPUT_CHANNEL);
	m_inst[pc].MakeNonBlocking();
	pc++;

	deltaLeftRead.Set(FIFO, m_partitionSizeInCL);
	m_inst[pc].Delta(m_partitionSizeInCL, deltaLeftRead, deltaRightRead, deltaWrite, (type == logreg));
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Dot(m_partitionSizeInCL, dotLeftRead, dotRightRead, dotWrite, false);
	m_inst[pc].Jump(0, m_numFeatures-1, pcModify, pc+1);
	pc++;
	// End---Innermost loop

	m_inst[pc].Modify(type, 1, scaledStepSize, scaledLambda, modelRead, modifyWrite);
	m_inst[pc].MakeNonBlocking();
	pc++;

	updateModelWrite.Set(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Update(m_partitionSizeInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	pc++;

	localaccess_t writebackResidualRead(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].WriteBack(true, m_residualChunk.m_offsetInCL, m_partitionSizeInCL,
		0, m_partitionSizeInCL, 0,
		true, writebackResidualRead, Instruction::WRITEBACK_MODEL_CHANNEL);
	pc++;

	localaccess_t writebackModelRead(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].WriteBack(true, m_modelChunk.m_offsetInCL, m_numFeaturesInCL,
		0, m_numFeaturesInCL, m_numPartitions*m_numFeaturesInCL,
		true, writebackModelRead, Instruction::WRITEBACK_LABELS_CHANNEL);
	m_inst[pc].Jump(1, partitionToStart+numPartitions-1, pcPartitionStart, pc+1);
	m_inst[pc].IncrementIndex(1);
	pc++;

	m_inst[pc].Jump(2, numEpochs-1, pcEpochStart, 0xFFFFFFFF);
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

bool FPGA_ColumnML::ReadBandwidth(uint32_t numLinesToRead, uint32_t numLinesToWrite, uint32_t numIterations) {

	bool fit = true;
	fit &= CheckMemoryFit(numLinesToRead, iFPGA::MEMORY_SIZE_IN_CL, "REGION_INPUT");
	if (fit == false) {
		return false;
	}

	m_ifpga->Realloc(m_inputHandle, numIterations*(numLinesToRead+numLinesToWrite)*64);
	m_base = iFPGA::CastToFloat(m_inputHandle);
	for (uint32_t i = 0; i < numIterations*numLinesToRead*16; i++) {
		m_base[i] = i;
	}
	uint32_t offset = numIterations*numLinesToRead*16;
	for (uint32_t i = 0; i < numIterations*numLinesToWrite*16; i++) {
		m_base[offset+i] = 0;
	}

	// *************************************************************************
	//
	//   START Program
	//
	// *************************************************************************
	uint32_t pc = 0;

	m_inst[pc].Prefetch(0, numIterations*numLinesToRead, 0, 0, 0);
	m_inst[pc].ResetIndex(2);
	m_inst[pc].MakeNonBlocking();
	pc++;

	uint32_t pcLoad = pc;

	localaccess_t readLines(0, numLinesToRead);
	localaccess_t writeLines(0, numLinesToWrite);
	if (numLinesToWrite == 0) {
		m_inst[pc].Load(0, numLinesToRead, 0, 0, numLinesToRead, readLines, Instruction::LOAD_REGION_LABELS_CHANNEL);
		pc++;
	}
	else if (numLinesToWrite == numLinesToRead) {
		readLines.Set(FIFO, numLinesToRead);
		writeLines.Set(FIFO, numLinesToWrite);

		m_inst[pc].Load(0, numLinesToRead, 0, 0, numLinesToRead, readLines, Instruction::LOAD_REGION_LABELS_CHANNEL);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].WriteBack(true, numIterations*numLinesToRead, numLinesToWrite, 0, 0, numLinesToWrite, true, writeLines, Instruction::WRITEBACK_LABELS_CHANNEL);
		pc++;
	}
	else {
		m_inst[pc].Load(0, numLinesToRead, 0, 0, numLinesToRead, readLines, Instruction::LOAD_REGION_LABELS_CHANNEL);
		pc++;

		m_inst[pc].WriteBack(true, numIterations*numLinesToRead, numLinesToWrite, 0, 0, numLinesToWrite, true, writeLines, Instruction::WRITEBACK_LABELS_CHANNEL);
		pc++;
	}

	m_inst[pc].Jump(2, numIterations-1, pcLoad, 0xFFFFFFFF);
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