#include "FPGA_ColumnML.h"

bool FPGA_ColumnML::fSGD(
	ModelType type,
	uint32_t numEpochs,
	float stepSize,
	float lambda)
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
	vector<localaccess_t> loadModelWrite(Instruction::NUM_LOAD_CHANNELS);
	loadModelWrite[Instruction::LOAD_REGION_MODEL_CHANNEL].Set(modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].ResetIndex(2);
	pc++;

	uint32_t beginEpoch = pc;

	localaccess_t modelCopy(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Copy(modelCopy, modelCopy);
	pc++;

	// Load labels in partition
	vector<localaccess_t> loadLabelsWrite(Instruction::NUM_LOAD_CHANNELS);
	loadLabelsWrite[Instruction::LOAD_REGION_LABELS_CHANNEL].Set(labelOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite);
	pc++;

	m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	vector<localaccess_t> loadSamplesWrite(Instruction::NUM_LOAD_CHANNELS);
	loadSamplesWrite[Instruction::LOAD_REGION_INPUT_CHANNEL].Set(FIFOBRAM, 0, m_numFeaturesInCL);
	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite);
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

	localaccess_t updateSamplesRead(BRAM, 0, m_numFeaturesInCL);
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

	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite);
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

		loadLabelsWrite[Instruction::LOAD_REGION_LABELS_CHANNEL].Set(labelOffsetInBRAM, m_restInCL);
		m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_restInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite);
		pc++;

		m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite);
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

		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite);
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
	vector<localaccess_t> writebackModelRead(Instruction::NUM_WRITEBACK_CHANNELS);
	writebackModelRead[Instruction::WRITEBACK_MODEL_CHANNEL].Set(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
		0, 0, m_numFeaturesInCL,
		0, true, writebackModelRead);
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
		0, true, writebackModelRead);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF0, 0xFFFFFFF0);
	pc++;

	// Context Load Instructions
	uint32_t pcContextLoad = pc;
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite);
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
	float lambda)
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
	vector<localaccess_t> loadModelWrite(Instruction::NUM_LOAD_CHANNELS);
	loadModelWrite[Instruction::LOAD_REGION_MODEL_CHANNEL].Set(modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].ResetIndex(2);
	pc++;

	localaccess_t modelCopy(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Copy(modelCopy, modelCopy);
	pc++;

	// Load labels in partition
	vector<localaccess_t> loadLabelsWrite(Instruction::NUM_LOAD_CHANNELS);
	loadLabelsWrite[Instruction::LOAD_REGION_LABELS_CHANNEL].Set(labelOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite);
	uint32_t pcLabels = pc;
	pc++;

	m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	// Start---Innermost loop
	vector<localaccess_t> loadSamplesWrite(Instruction::NUM_LOAD_CHANNELS);
	loadSamplesWrite[Instruction::LOAD_REGION_INPUT_CHANNEL].Set(FIFOBRAM, 0, minibatchSize*m_numFeaturesInCL);
	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, minibatchSize*m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite);
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

	// localaccess_t updateSamplesRead(BRAM, 0, m_numFeaturesInCL, true);
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
		loadLabelsWrite[Instruction::LOAD_REGION_LABELS_CHANNEL].Set(labelOffsetInBRAM, m_restInCL);
		m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_restInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite);
		pc++;

		// Start---Innermost loop
		loadSamplesWrite[Instruction::LOAD_REGION_INPUT_CHANNEL].Set(FIFOBRAM, 0, m_rest*m_numFeaturesInCL);
		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, loadSamplesWrite);
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
	vector<localaccess_t> writebackModelRead(Instruction::NUM_WRITEBACK_CHANNELS);
	writebackModelRead[Instruction::WRITEBACK_MODEL_CHANNEL].Set(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
		0, 0, m_numFeaturesInCL,
		0, true, writebackModelRead);
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
		0, true, writebackModelRead);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF0, 0xFFFFFFF0);
	pc++;

	// Context Load Instructions
	uint32_t pcContextLoad = pc;
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, loadModelWrite);
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

	// Load residual
	vector<localaccess_t> loadResidualWrite(Instruction::NUM_LOAD_CHANNELS);
	loadResidualWrite[Instruction::LOAD_REGION_MODEL_CHANNEL].Set(residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_residualChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadResidualWrite);
	pc++;

	// Load labels in partition
	vector<localaccess_t> loadLabelsWrite(Instruction::NUM_LOAD_CHANNELS);
	loadLabelsWrite[Instruction::LOAD_REGION_LABELS_CHANNEL].Set(labelOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, loadLabelsWrite);
	pc++;

	// Load model
	vector<localaccess_t> loadModelWrite(Instruction::NUM_LOAD_CHANNELS);
	loadModelWrite[Instruction::LOAD_REGION_LABELS_CHANNEL].Set(modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_numFeaturesInCL, 0, m_numFeaturesInCL, m_numPartitions*m_numFeaturesInCL, loadModelWrite);
	pc++;

	// Load accessprops
	vector<localaccess_t> loadAccesspropsWrite(Instruction::NUM_LOAD_CHANNELS);
	loadAccesspropsWrite[Instruction::LOAD_MEM_ACCESSPROPS_CHANNEL].Set(accesspropsOffsetInBRAM, 2*m_numFeaturesInCL);
	m_inst[pc].Load(m_accesspropsChunk.m_offsetInCL, 2*m_numFeaturesInCL, 0, 2*m_numFeaturesInCL, 0, loadAccesspropsWrite);
	pc++;

	// Load samples
	vector<localaccess_t> loadSamplesWrite(Instruction::NUM_LOAD_CHANNELS);
	loadSamplesWrite[Instruction::LOAD_REGION_INPUT_CHANNEL].Set(FIFOBRAM, 0, m_partitionSizeInCL);
	m_inst[pc].LocalLoad(0, 1, 0, 0, loadSamplesWrite);
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

	localaccess_t updateSamplesRead(BRAM, 0, m_partitionSizeInCL);
	localaccess_t updateModelRead(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	localaccess_t updateModelWrite(FIFOBRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Update(m_partitionSizeInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	m_inst[pc].MakeNonBlocking();
	m_inst[pc].IncrementIndex(0);
	pc++;

	m_inst[pc].LocalLoad(0, 1, 0, 0, loadSamplesWrite);
	m_inst[pc].MakeNonBlocking();
	pc++;

	deltaLeftRead.Set(FIFO, m_partitionSizeInCL);
	m_inst[pc].Delta(m_partitionSizeInCL, deltaLeftRead, deltaRightRead, deltaWrite, (type == logreg));
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Dot(m_partitionSizeInCL, dotLeftRead, dotRightRead, dotWrite, false);
	m_inst[pc].Jump(0, m_cstore->m_numFeatures-1, pcModify, pc+1);
	pc++;
	// End---Innermost loop

	m_inst[pc].Modify(type, 1, scaledStepSize, scaledLambda, modelRead, modifyWrite);
	m_inst[pc].MakeNonBlocking();
	pc++;

	updateModelWrite.Set(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].Update(m_partitionSizeInCL, updateSamplesRead, modifyWrite, updateModelRead, updateModelWrite, false);
	pc++;

	vector<localaccess_t> writebackResidualRead(Instruction::NUM_WRITEBACK_CHANNELS);
	writebackResidualRead[Instruction::WRITEBACK_MODEL_CHANNEL].Set(BRAM, residualOffsetInBRAM, m_partitionSizeInCL);
	m_inst[pc].WriteBack(true, m_residualChunk.m_offsetInCL, m_partitionSizeInCL,
		0, m_partitionSizeInCL, 0,
		0, true, writebackResidualRead);
	pc++;

	vector<localaccess_t> writebackModelRead(Instruction::NUM_WRITEBACK_CHANNELS);
	writebackModelRead[Instruction::WRITEBACK_LABELS_CHANNEL].Set(BRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].WriteBack(true, m_modelChunk.m_offsetInCL, m_numFeaturesInCL,
		0, m_numFeaturesInCL, m_numPartitions*m_numFeaturesInCL,
		1, true, writebackModelRead);
	m_inst[pc].Jump(1, m_numPartitions-1, 0, pc+1);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].IncrementIndex(1);
	pc++;

	m_inst[pc].Jump(2, numEpochs-1, 0, 0xFFFFFFFF);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
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

/*
bool FPGA_ColumnML::fSGD_blocking(
	ModelType type,
	uint32_t numEpochs,
	float stepSize,
	float lambda)
{
	if (m_base == nullptr) {
		cout << "m_base is nullptr!" << endl;
		return false;
	}

	uint32_t modelOffsetInBRAM = 0;
	uint32_t labelOffsetInBRAM = 0;

	AccessProperties accessModel(5);
	accessModel.Set(2, modelOffsetInBRAM, m_modelChunk.m_lengthInCL);

	AccessProperties accessLabels(5);
	accessLabels.Set(3, labelOffsetInBRAM, m_partitionSizeInCL);

	AccessProperties accessSamples(5);
	accessSamples.Set(0, 0, m_numFeaturesInCL);
	accessSamples.Set(1, 0, m_numFeaturesInCL);

	AccessProperties writebackModel(2);
	writebackModel.Set(0, modelOffsetInBRAM, m_numFeaturesInCL);

	// *************************************************************************
	//
	//   START Program
	//
	// *************************************************************************
	uint32_t pc = 0;

	// Load model
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, accessModel);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].ResetIndex(2);
	pc++;

	// Load labels in partition
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessLabels);
	uint32_t pcLabels = pc;
	pc++;

	// Start---Innermost loop
	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_numFeaturesInCL*m_partitionSize, 0, accessSamples);
	m_inst[pc].MakeNonBlocking();
	uint32_t pcSamples = pc;
	pc++;

	m_inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	pc++;

	m_inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
	pc++;

	m_inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
	pc++;

	m_inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
	m_inst[pc].Jump(0, m_partitionSize-1, pcSamples, pc+1);
	m_inst[pc].IncrementIndex(0);
	pc++;
	// End---Innermost loop

	m_inst[pc].Jump(1, m_numPartitions-1, pcLabels, pc+1);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].IncrementIndex(1);
	pc++;

	if ( m_rest > 0 ) {
		accessLabels.Set(3, labelOffsetInBRAM, m_restInCL);
		m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_restInCL, 0, m_partitionSizeInCL, 0, accessLabels);
		pc++;

		// Start---Innermost loop
		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_numFeaturesInCL*m_partitionSize, 0, accessSamples);
		m_inst[pc].MakeNonBlocking();
		uint32_t pcRestSamples = pc;
		pc++;

		m_inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		pc++;

		m_inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		pc++;

		m_inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
		m_inst[pc].Jump(0, m_rest-1, pcRestSamples, pc+1);
		m_inst[pc].IncrementIndex(0);
		pc++;
		// End---Innermost loop
	}

	// WriteBack
	m_inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
		0, 0, m_numFeaturesInCL,
		0, true, writebackModel);
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
		0, true, writebackModel);
	pc++;

	m_inst[pc].Jump(2, 0, 0xFFFFFFF0, 0xFFFFFFF0);
	pc++;

	// Context Load Instructions
	uint32_t pcContextLoad = pc;
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_modelChunk.m_lengthInCL, 0, 0, 0, accessModel);
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

void FPGA_ColumnML::ReadBandwidth(uint32_t numIterations) {
	uint32_t numLines = 2048;

	AccessProperties accessSamples(5);
	accessSamples.Set(2, 0, numLines);

	uint32_t pc = 0;

	m_inst[pc].Prefetch(0, numIterations*numLines, 0, 0, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Load(0, numLines, 0, 0, 0, accessSamples);
	uint32_t pcLoad = pc;
	pc++;

	m_inst[pc].Jump(2, numIterations-1, pcLoad, 0xFFFFFFFF);
	m_inst[pc].IncrementIndex(2);
	pc++;

	m_ifpga->Realloc(m_inputHandle, numIterations*numLines*64);
	m_outputSizeInCL = 1;
	m_ifpga->Realloc(m_outputHandle, m_outputSizeInCL*64);

	m_numInstructions = pc;
	WriteProgramMemory(0, 0);
}

void FPGA_ColumnML::Correctness() {
	uint32_t numLines = 4;

	AccessProperties accessRead(5);
	accessRead.Set(2, 0, numLines);

	AccessProperties accessWriteback(2);
	accessWriteback.Set(0, 0, numLines);

	uint32_t pc = 0;

	m_inst[pc].Load(0, numLines, 0, 0, 0, accessRead);
	uint32_t pcLoad = pc;
	pc++;

	m_inst[pc].WriteBack(0, 1, numLines, 0, 0, 0, 0, true, accessWriteback);
	pc++;

	m_inst[pc].Jump(2, 0, pcLoad, 0xFFFFFFFF);
	pc++;

	m_ifpga->Realloc(m_inputHandle, numLines*64);
	auto input = iFPGA::CastToInt(m_inputHandle);
	for (uint32_t i = 0; i < numLines*16; i++) {
		input[i] = i+1;
	}

	m_outputSizeInCL = (numLines+1);
	m_ifpga->Realloc(m_outputHandle, m_outputSizeInCL*64);
	auto output = iFPGA::CastToInt(m_outputHandle);
	for (uint32_t i = 0; i < (numLines+1)*16; i++) {
		output[i] = 0;
	}

	m_numInstructions = pc;
	WriteProgramMemory(0, 0);
}
*/