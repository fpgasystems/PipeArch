#include "FPGA_ColumnML.h"

bool FPGA_ColumnML::fSGD(
	ModelType type,
	uint32_t numEpochs,
	float stepSize,
	float lambda)
{
	if (m_memory == nullptr) {
		cout << "m_memory is nullptr!" << endl;
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

	uint32_t beginEpoch = pc;
	
	m_inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	m_inst[pc].MakeNonBlocking();
	pc++;

	// Load labels in partition
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessLabels);
	pc++;

	m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
	pc++;

	// Start---Innermost loop
	m_inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
	m_inst[pc].MakeNonBlocking();
	uint32_t pcModify = pc;
	pc++;

	m_inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, true);
	m_inst[pc].MakeNonBlocking();
	m_inst[pc].IncrementIndex(0);
	pc++;

	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Dot(m_numFeaturesInCL, true, false, modelOffsetInBRAM, 0xFFFF);
	m_inst[pc].Jump(0, m_partitionSize-1, pcModify, pc+1);
	pc++;
	// End---Innermost loop

	m_inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
	m_inst[pc].Jump(1, m_numPartitions-1, beginEpoch, pc+1);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].IncrementIndex(1);
	pc++;

	if ( m_rest > 1 ) {
		accessLabels.Set(3, labelOffsetInBRAM, m_restInCL);

		m_inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_restInCL, 0, m_partitionSizeInCL, 0, accessLabels);
		pc++;

		m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Dot(m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		pc++;

		// Start---Innermost loop
		m_inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		m_inst[pc].MakeNonBlocking();
		uint32_t pcRestSamples = pc;
		pc++;

		m_inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, true);
		m_inst[pc].MakeNonBlocking();
		m_inst[pc].IncrementIndex(0);
		pc++;

		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, accessSamples);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Dot(m_numFeaturesInCL, true, false, modelOffsetInBRAM, 0xFFFF);
		m_inst[pc].Jump(0, m_rest-1, pcRestSamples, pc+1);
		pc++;
		// End---Innermost loop

		m_inst[pc].Modify(labelOffsetInBRAM, type, 0, stepSize, lambda);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Update(modelOffsetInBRAM, m_numFeaturesInCL, false);
		pc++;
	}

	// WriteBack
	m_inst[pc].WriteBack(false, 1, m_numFeaturesInCL,
		0, 0, m_numFeaturesInCL,
		0, true, writebackModel);
	pc++;

	m_inst[pc].Jump(2, numEpochs-1, beginEpoch, 0xFFFFFFFF);
	m_inst[pc].ResetIndex(0);
	m_inst[pc].ResetIndex(1);
	m_inst[pc].IncrementIndex(2);
	pc++;

	// *************************************************************************
	//
	//   END Program
	//
	// *************************************************************************
	m_numInstructions = pc;
	WriteProgramMemory();

	m_outputSizeInCL = (numEpochs*m_numFeaturesInCL+1);
	realloc(m_outputHandle, m_outputSizeInCL*64);

	return true;
}

bool FPGA_ColumnML::fSGD_minibatch(
	ModelType type,
	uint32_t numEpochs,
	uint32_t minibatchSize, 
	float stepSize,
	float lambda)
{
	if (m_memory == nullptr) {
		cout << "m_memory is nullptr!" << endl;
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

	uint32_t modelOffsetInBRAM = 0;
	uint32_t labelOffsetInBRAM = 0;

	AccessProperties accessModel(5);
	accessModel.Set(2, modelOffsetInBRAM, m_modelChunk.m_lengthInCL);

	AccessProperties accessLabels(5);
	accessLabels.Set(3, labelOffsetInBRAM, m_partitionSizeInCL);

	AccessProperties minibatchAccessSamples(5);
	minibatchAccessSamples.Set(0, 0, minibatchSize*m_numFeaturesInCL);
	minibatchAccessSamples.Set(1, 0, minibatchSize*m_numFeaturesInCL);

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

	m_inst[pc].Prefetch(m_samplesChunk.m_offsetInCL, m_partitionSize*m_numFeaturesInCL, 0, m_partitionSize*m_numFeaturesInCL, 0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	// Start---Innermost loop
	m_inst[pc].Load(m_samplesChunk.m_offsetInCL, minibatchSize*m_numFeaturesInCL, m_numFeaturesInCL, m_partitionSize*m_numFeaturesInCL, 0, minibatchAccessSamples);
	uint32_t pcSamples = pc;
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
	pc++;

	m_inst[pc].Dot(minibatchSize, m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Modify(minibatchSize, labelOffsetInBRAM, type, 0, scaledStepSize, lambda);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Update(minibatchSize, modelOffsetInBRAM, m_numFeaturesInCL, false);
	m_inst[pc].IncrementIndex(0, minibatchSize);
	m_inst[pc].Jump(0, m_partitionSize-minibatchSize, pcSamples, pc+1);
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
		minibatchAccessSamples.Set(0, 0, m_rest*m_numFeaturesInCL);
		minibatchAccessSamples.Set(1, 0, m_rest*m_numFeaturesInCL);
		m_inst[pc].Load(m_samplesChunk.m_offsetInCL, m_rest*m_numFeaturesInCL, m_numFeaturesInCL, m_numFeaturesInCL*m_partitionSize, 0, minibatchAccessSamples);
		m_inst[pc].MakeNonBlocking();
		uint32_t pcRestSamples = pc;
		pc++;

		m_inst[pc].Copy(modelOffsetInBRAM, modelOffsetInBRAM, m_numFeaturesInCL);
		pc++;

		m_inst[pc].Dot(m_rest, m_numFeaturesInCL, false, false, modelOffsetInBRAM, 0xFFFF);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Modify(m_rest, labelOffsetInBRAM, type, 0, scaledStepSize, lambda);
		m_inst[pc].MakeNonBlocking();
		pc++;

		m_inst[pc].Update(m_rest, modelOffsetInBRAM, m_numFeaturesInCL, false);
		m_inst[pc].Jump(0, 0, pcRestSamples, pc+1);
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

	// *************************************************************************
	//
	//   END Program
	//
	// *************************************************************************
	m_numInstructions = pc;
	WriteProgramMemory();

	m_outputSizeInCL = (numEpochs*m_numFeaturesInCL+1);
	realloc(m_outputHandle, m_outputSizeInCL*64);

	return true;
}

bool FPGA_ColumnML::fSCD(
	ModelType type, 
	uint32_t numEpochs,
	float stepSize, 
	float lambda)
{
	if (m_memory == nullptr) {
		cout << "m_memory is nullptr!" << endl;
		return false;
	}

	float scaledStepSize = stepSize/m_partitionSize;
	float scaledLambda = stepSize*lambda;

	uint32_t residualOffsetInBRAM = 0;
	uint32_t labelOffsetInBRAM = 0;
	uint32_t modelOffsetInBRAM = labelOffsetInBRAM + m_partitionSizeInCL;
	uint32_t accesspropsOffsetInBRAM = 0;

	AccessProperties accessResidual(5);
	accessResidual.Set(2, residualOffsetInBRAM, m_partitionSizeInCL);

	AccessProperties accessLabels(5);
	accessLabels.Set(3, labelOffsetInBRAM, m_partitionSizeInCL);

	AccessProperties accessModel(5);
	accessModel.Set(3, modelOffsetInBRAM, m_numFeaturesInCL);

	AccessProperties accessSamples(5);
	accessSamples.Set(0, 0, m_partitionSizeInCL);
	accessSamples.Set(1, 0, m_partitionSizeInCL);

	AccessProperties accessAccessProps(5);
	accessAccessProps.Set(4, accesspropsOffsetInBRAM, m_numFeaturesInCL*2);

	AccessProperties writebackResidual(2);
	writebackResidual.Set(0, residualOffsetInBRAM, m_partitionSizeInCL);

	AccessProperties writebackModel(2);
	writebackModel.Set(1, modelOffsetInBRAM, m_numFeaturesInCL);


	// *************************************************************************
	//
	//   START Program
	//
	// *************************************************************************
	uint32_t pc = 0;

	// Load residual
	m_inst[pc].Load(m_residualChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessResidual);
	pc++;

	// Load labels in partition
	m_inst[pc].Load(m_labelsChunk.m_offsetInCL, m_partitionSizeInCL, 0, m_partitionSizeInCL, 0, accessLabels);
	pc++;

	// Load model
	m_inst[pc].Load(m_modelChunk.m_offsetInCL, m_numFeaturesInCL, 0, m_numFeaturesInCL, 0, accessModel);
	pc++;

	// Load accessprops
	m_inst[pc].Load(m_accesspropsChunk.m_offsetInCL, m_numFeaturesInCL*2, 0, m_numFeaturesInCL*2, 0, accessAccessProps);
	pc++;

	// Load samples
	m_inst[pc].LocalLoad(0, 1, 0, 0, accessSamples);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Dot(m_partitionSizeInCL, false, true, residualOffsetInBRAM, labelOffsetInBRAM);
	pc++;

	// Start---Innermost loop
	m_inst[pc].Modify(modelOffsetInBRAM, type, 1, scaledStepSize, scaledLambda);
	m_inst[pc].MakeNonBlocking();
	uint32_t pcModify = pc;
	pc++;

	m_inst[pc].Update(residualOffsetInBRAM, m_partitionSizeInCL, true);
	m_inst[pc].IncrementIndex(0);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].LocalLoad(0, 1, 0, 0, accessSamples);
	m_inst[pc].MakeNonBlocking();
	pc++;

	m_inst[pc].Dot(m_partitionSizeInCL, true, true, residualOffsetInBRAM, labelOffsetInBRAM);
	m_inst[pc].Jump(0, m_cstore->m_numFeatures-1, pcModify, pc+1);
	pc++;
	// End---Innermost loop

	m_inst[pc].Modify(modelOffsetInBRAM, type, 1, scaledStepSize, scaledLambda);
	pc++;

	m_inst[pc].Update(residualOffsetInBRAM, m_partitionSizeInCL, false);
	pc++;

	m_inst[pc].WriteBack(true, m_residualChunk.m_offsetInCL, m_partitionSizeInCL,
		0, m_partitionSizeInCL, 0,
		0, true, writebackResidual);
	pc++;

	m_inst[pc].WriteBack(true, m_modelChunk.m_offsetInCL, m_numFeaturesInCL,
		0, m_numFeaturesInCL, 0,
		1, true, writebackModel);
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
	m_numInstructions = pc;
	WriteProgramMemory();

	m_outputSizeInCL = 1;
	realloc(m_outputHandle, m_outputSizeInCL*64);

	return true;
}

bool FPGA_ColumnML::fSGD_blocking(
	ModelType type,
	uint32_t numEpochs,
	float stepSize,
	float lambda)
{
	if (m_memory == nullptr) {
		cout << "m_memory is nullptr!" << endl;
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

	// *************************************************************************
	//
	//   END Program
	//
	// *************************************************************************
	m_numInstructions = pc;
	WriteProgramMemory();

	m_outputSizeInCL = (numEpochs*m_numFeaturesInCL+1);
	realloc(m_outputHandle, m_outputSizeInCL*64);

	return true;
}