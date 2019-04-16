#pragma once

enum DataSource {BRAM, FIFO, FIFOBRAM};

class access_t {
public:
	bool m_bram;
	bool m_fifo;
	uint32_t m_offsetInCL;
	uint32_t m_lengthInCL;
	bool m_keepCountAlongIterations;

	access_t()  {
		m_bram = false;
		m_fifo = false;
		m_offsetInCL = 0;
		m_lengthInCL = 0;
		m_keepCountAlongIterations = false;
	}

	access_t(uint32_t offsetInCL, uint32_t lengthInCL)  {
		m_bram = true;
		m_fifo = false;
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = false;
	}

	access_t(DataSource source, uint32_t offsetInCL, uint32_t lengthInCL)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = false;
	}

	access_t(DataSource source, uint32_t offsetInCL, uint32_t lengthInCL, bool keepCountAlongIterations)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = keepCountAlongIterations;
	}

	access_t(DataSource source, uint32_t lengthInCL)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = 0;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = false;
	}

	void Set(uint32_t offsetInCL, uint32_t lengthInCL) {
		m_bram = true;
		m_fifo = false;
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
	}

	void Set(DataSource source, uint32_t offsetInCL, uint32_t lengthInCL) {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
	}

	void Set(DataSource source, uint32_t lengthInCL) {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_lengthInCL = lengthInCL;
	}

	uint32_t GetReg() {
		return	((m_fifo ? 1:0) << 31) |
				((m_bram ? 1:0) << 30) |
				((m_lengthInCL & 0xFFFF) << 16) |
				((m_keepCountAlongIterations ? 1:0) << 15) |
				(m_offsetInCL & 0xFFFF);
	}
};

class Instruction {
public:
	static const uint32_t MAX_NUM_INSTRUCTIONS = 64;
	static const uint32_t NUM_WORDS = 16;
	static const uint32_t NUM_BYTES = NUM_WORDS*4;
	uint32_t m_data[NUM_WORDS];

	static const uint32_t NUM_LOAD_CHANNELS = 4;
	static const uint32_t LOAD_REGION_INPUT_CHANNEL = 0;
	static const uint32_t LOAD_REGION_MODEL_CHANNEL = 1;
	static const uint32_t LOAD_REGION_LABELS_CHANNEL = 2;
	static const uint32_t LOAD_MEM_ACCESSPROPS_CHANNEL = 3;

	static const uint32_t NUM_WRITEBACK_CHANNELS = 2;
	static const uint32_t WRITEBACK_MODEL_CHANNEL = 0;
	static const uint32_t WRITEBACK_LABELS_CHANNEL = 1;

	Instruction() {
		for (unsigned i = 0; i < 16; i++) {
			m_data[i] = 0;
		}
		// Maintain indexes by default
		m_data[0] = 0xEFFFFFFF;
		m_data[1] = 0xEFFFFFFF;
		m_data[2] = 0xEFFFFFFF;
	}

	void LoadInstruction(volatile uint32_t* data) {
		for (uint32_t i = 0; i < NUM_WORDS; i++) {
			data[i] = m_data[i];
		}
	}

	void SetIndex(uint32_t whichIndex, uint32_t i) {
		m_data[whichIndex] = i;
	}

	void ResetIndex(uint32_t whichIndex) {
		m_data[whichIndex] = 0;
	}

	void IncrementIndex(uint32_t whichIndex) {
		m_data[whichIndex] = 0xFFFFFFF;
	}

	void IncrementIndex(uint32_t whichIndex, uint32_t amount) {
		m_data[whichIndex] = (1 << 31) | (amount & 0xEFFFFFFF);
	}

	void DecrementIndex(uint32_t whichIndex) {
		m_data[whichIndex] = 0x1FFFFFF;
	}

	void MakeNonBlocking() {
		m_data[15] |= (1 << 8);
	}

	void EnableContextSwitch() {
		m_data[15] |= (1 << 9);
	}

	void Jump(
		uint32_t whichReg,
		uint32_t predicate,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] |= (whichReg+1); // opcode
		m_data[13] = predicate;
		m_data[14] = ((nextPCFalse & 0xFFFF) << 16) | (nextPCTrue & 0xFFFF);
	}

	void JumpIfEven(
		uint32_t whichReg,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] |= (whichReg+1); // opcode
		m_data[13] = (1 << 30);
		m_data[14] = ((nextPCFalse & 0xFFFF) << 16) | (nextPCTrue & 0xFFFF);
	}

	void Prefetch(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2)
	{
		uint32_t enableMultiline = 1;
		m_data[15] |= (1 << 4);
		m_data[3] = loadOffsetDRAM;
		m_data[4] = (enableMultiline << 31) | loadLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		MakeNonBlocking();
	}

	void Load(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2,
		vector<access_t> loadAccess)
	{
		uint32_t enableMultiline = 1;
		m_data[15] |= (2 << 4);
		m_data[3] = loadOffsetDRAM;
		m_data[4] = (enableMultiline << 31) | loadLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		for (uint32_t i = 0; i < loadAccess.size(); i++) {
			m_data[5+i] = loadAccess[i].GetReg();
		}
	}

	void LocalLoad(
		uint32_t loadOffsetAccessProps,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2,
		vector<access_t> loadAccess)
	{
		uint32_t enableMultiline = 1;
		uint32_t useLocalAccessProps = 1;
		m_data[15] |= (2 << 4);
		m_data[3] = (useLocalAccessProps << 31) | loadOffsetAccessProps;
		m_data[4] = (enableMultiline << 31);
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		for (uint32_t i = 0; i < loadAccess.size(); i++) {
			m_data[5+i] = loadAccess[i].GetReg();
		}
	}

	void WriteBack(
		bool useInputSpace,
		uint32_t storeOffsetDRAM,
		uint32_t storeLengthDRAM,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2,
		uint32_t whichChannel,
		bool writeFence,
		vector<access_t> writebackAccess)
	{
		m_data[15] |= (3 << 4);
		m_data[3] = ((uint32_t)useInputSpace << 31) | storeOffsetDRAM;
		m_data[4] = storeLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		m_data[5] = ((uint32_t)writeFence << 4) | (whichChannel & 0xFFFF);
		for (uint32_t i = 0; i < writebackAccess.size(); i++) {
			m_data[6+i] = writebackAccess[i].GetReg();
		}
	}

	// *************************************************************************
	//
	//   Instructions
	//
	// *************************************************************************

	void Dot(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		access_t leftInputAccess,
		access_t rightInputAccess,
		access_t outputAccess)
	{
		m_data[15] |= (4 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
	}

	void Dot(
		uint32_t numLinesToProcess,
		access_t leftInputAccess,
		access_t rightInputAccess,
		access_t outputAccess)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (4 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
	}

	void Delta(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		access_t leftInputAccess,
		access_t rightInputAccess,
		access_t outputAccess)
	{
		m_data[15] |= (9 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
	}

	void Delta(
		uint32_t numLinesToProcess,
		access_t leftInputAccess,
		access_t rightInputAccess,
		access_t outputAccess)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (9 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
	}

	void Modify(
		uint32_t numIterations,
		uint32_t type, uint32_t algo, float stepSize, float lambda,
		access_t labelsInputAccess,
		access_t gradientOutputAccess)
	{
		m_data[15] |= (5 << 4);
		m_data[3] = labelsInputAccess.GetReg();
		m_data[4] = (numIterations << 16) | (algo << 2) | (type & 0x3);
		m_data[5] = *((uint32_t*)&stepSize);
		m_data[6] = *((uint32_t*)&lambda);
		m_data[7] = gradientOutputAccess.GetReg();
	}

	void Modify(
		uint32_t type, uint32_t algo, float stepSize, float lambda,
		access_t labelsInputAccess,
		access_t gradientOutputAccess)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (5 << 4);
		m_data[3] = labelsInputAccess.GetReg();
		m_data[4] = (numIterations << 16) | (algo << 2) | (type & 0x3);
		m_data[5] = *((uint32_t*)&stepSize);
		m_data[6] = *((uint32_t*)&lambda);
		m_data[7] = gradientOutputAccess.GetReg();
	}

	void Update(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		access_t samplesInputAccess,
		access_t gradientInputAccess,
		access_t modelReadAccess,
		access_t modelWriteAccess)
	{
		m_data[15] |= (6 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = samplesInputAccess.GetReg();
		m_data[5] = gradientInputAccess.GetReg();
		m_data[6] = modelReadAccess.GetReg();
		m_data[7] = modelWriteAccess.GetReg();
	}

	void Update(
		uint32_t numLinesToProcess,
		access_t samplesInputAccess,
		access_t gradientInputAccess,
		access_t modelReadAccess,
		access_t modelWriteAccess)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (6 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = samplesInputAccess.GetReg();
		m_data[5] = gradientInputAccess.GetReg();
		m_data[6] = modelReadAccess.GetReg();
		m_data[7] = modelWriteAccess.GetReg();
	}

	void Copy(
		access_t sourceInputAccess,
		access_t destinationOutputAccess)
	{
		m_data[15] |= (7 << 4);
		m_data[3] = sourceInputAccess.GetReg();
		m_data[4] = destinationOutputAccess.GetReg();
	}

	void Sync()
	{
		m_data[15] |= (8 << 4);
	}
};