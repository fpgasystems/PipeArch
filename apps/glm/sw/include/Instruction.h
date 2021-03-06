#pragma once

enum DataSource {BRAM, FIFO, FIFOBRAM};

class remoteaccess_t {
public:
	uint32_t m_offsetInCL;
	uint32_t m_lengthInCL;
};

class localaccess_t {
public:
	bool m_bram;
	bool m_fifo;
	uint32_t m_offsetInCL;
	uint32_t m_lengthInCL;
	bool m_keepCountAlongIterations;
	bool m_useLocalProps;

	localaccess_t()  {
		m_bram = false;
		m_fifo = false;
		m_offsetInCL = 0;
		m_lengthInCL = 0;
		m_keepCountAlongIterations = false;
		m_useLocalProps = false;
	}

	localaccess_t(uint32_t offsetInCL, uint32_t lengthInCL)  {
		m_bram = true;
		m_fifo = false;
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = false;
		m_useLocalProps = false;
	}

	localaccess_t(DataSource source, uint32_t offsetInCL, uint32_t lengthInCL)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = false;
		m_useLocalProps = false;
	}

	localaccess_t(DataSource source, uint32_t offsetInCL, uint32_t lengthInCL, bool keepCountAlongIterations)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = keepCountAlongIterations;
		m_useLocalProps = false;
	}

	localaccess_t(DataSource source, uint32_t offsetInCL, uint32_t lengthInCL, bool keepCountAlongIterations, bool useLocalAccessProps)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = offsetInCL;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = keepCountAlongIterations;
		m_useLocalProps = useLocalAccessProps;
	}

	localaccess_t(DataSource source, uint32_t lengthInCL)  {
		m_bram = (source == BRAM || source == FIFOBRAM);
		m_fifo = (source == FIFO || source == FIFOBRAM);
		m_offsetInCL = 0;
		m_lengthInCL = lengthInCL;
		m_keepCountAlongIterations = false;
		m_useLocalProps = false;
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
				((m_lengthInCL & 0x3FFF) << 16) |
				((m_keepCountAlongIterations ? 1:0) << 15) |
				((m_useLocalProps << 14)) |
				(m_offsetInCL & 0x3FFF);
	}
};

class Instruction {
public:
	static const uint32_t MAX_NUM_INSTRUCTIONS = 64;
	static const uint32_t NUM_WORDS = 16;
	static const uint32_t NUM_BYTES = NUM_WORDS*4;
	uint32_t m_data[NUM_WORDS];

	static const uint32_t LOAD_REGION_INPUT_CHANNEL = 1;
	static const uint32_t LOAD_REGION_MODEL_CHANNEL = (1 << 1);
	static const uint32_t LOAD_REGION_LABELS_CHANNEL = (1 << 2);
	static const uint32_t LOAD_MEM_ACCESSPROPS_CHANNEL = (1 << 3);
	static const uint32_t LOAD_MEM_LOCALPROPS_CHANNEL = (1 << 4);
	static const uint32_t LOAD_REGION_PREFETCH_CHANNEL = (1 << 5);

	static const uint32_t WRITEBACK_MODEL_CHANNEL = 0;
	static const uint32_t WRITEBACK_LABELS_CHANNEL = 1;
	static const uint32_t WRITEBACK_INPUT_CHANNEL = 2;

	static const uint32_t PREFETCH_REGION_INPUT_CHANNEL = 1;
	static const uint32_t PREFETCH_REGION_INPUTCOPY_CHANNEL = (1 << 1);
	static const uint32_t PREFETCH_REGION_LABELS_CHANNEL = (1 << 2);

	static const uint32_t USE_REG = 0xFFFFFFFF;

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
		localaccess_t loadAccess,
		uint32_t channelSelection)
	{
		uint32_t enableMultiline = 1;
		m_data[15] |= (2 << 4);
		m_data[3] = loadOffsetDRAM;
		m_data[4] = (enableMultiline << 31) | loadLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		m_data[5] = loadAccess.GetReg();
		m_data[6] = channelSelection;
	}

	void LocalLoad(
		uint32_t loadOffsetAccessProps,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2,
		localaccess_t loadAccess,
		uint32_t channelSelection)
	{
		uint32_t enableMultiline = 1;
		uint32_t useLocalAccessProps = 1;
		m_data[15] |= (2 << 4);
		m_data[3] = (useLocalAccessProps << 31) | loadOffsetAccessProps;
		m_data[4] = (enableMultiline << 31);
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		m_data[5] = loadAccess.GetReg();
		m_data[6] = channelSelection;
	}

	void WriteBack(
		bool useInputSpace,
		uint32_t storeOffsetDRAM,
		uint32_t storeLengthDRAM,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2,
		bool writeFence,
		localaccess_t writebackAccess,
		uint32_t whichChannel)
	{
		m_data[15] |= (3 << 4);
		m_data[3] = ((uint32_t)useInputSpace << 31) | storeOffsetDRAM;
		m_data[4] = storeLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		m_data[5] = ((uint32_t)writeFence << 4) | (whichChannel & 0xFFFF);
		m_data[6] = writebackAccess.GetReg();
	}

	void BlockOnInstruction(string name) {
		m_data[15] |= (1 << 10) | (1 << 8);
		
		if (name.compare("Load") == 0)
			m_data[15] |= (2 << 4);
		else if (name.compare("WriteBack") == 0)
			m_data[15] |= (3 << 4);
		else if (name.compare("Dot") == 0)
			m_data[15] |= (4 << 4);
		else if (name.compare("Delta") == 0)
			m_data[15] |= (9 << 4);
		else if (name.compare("Modify") == 0)
			m_data[15] |= (5 << 4);
		else if (name.compare("Update") == 0)
			m_data[15] |= (6 << 4);
		else if (name.compare("Update2") == 0)
			m_data[15] |= (10 << 4);
		else if (name.compare("L2Reg") == 0)
			m_data[15] |= (12 << 4);
		else if (name.compare("Copy") == 0)
			m_data[15] |= (12 << 4);
		else if (name.compare("CopyPrefetch") == 0)
			m_data[15] |= (7 << 4);
	}

	// *************************************************************************
	//
	//   Instructions
	//
	// *************************************************************************

	void Dot(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		localaccess_t leftInputAccess,
		localaccess_t rightInputAccess,
		localaccess_t outputAccess,
		bool doSigmoid)
	{
		m_data[15] |= (4 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
		m_data[7] = doSigmoid ? 1 : 0;
	}

	void Dot(
		uint32_t numLinesToProcess,
		localaccess_t leftInputAccess,
		localaccess_t rightInputAccess,
		localaccess_t outputAccess,
		bool doSigmoid)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (4 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
		m_data[7] = doSigmoid ? 1 : 0;
	}

	void Delta(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		localaccess_t leftInputAccess,
		localaccess_t rightInputAccess,
		localaccess_t outputAccess,
		bool doSigmoid)
	{
		m_data[15] |= (9 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
		m_data[7] = doSigmoid ? 1 : 0;
	}

	void Delta(
		uint32_t numLinesToProcess,
		localaccess_t leftInputAccess,
		localaccess_t rightInputAccess,
		localaccess_t outputAccess,
		bool doSigmoid)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (9 << 4);
		m_data[3] = ((numIterations & 0xFFFF) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = leftInputAccess.GetReg();
		m_data[5] = rightInputAccess.GetReg();
		m_data[6] = outputAccess.GetReg();
		m_data[7] = doSigmoid ? 1 : 0;
	}

	void Modify(
		bool useIndex0,
		uint32_t numIterations,
		uint32_t type, uint32_t algo, float stepSize, float lambda,
		localaccess_t labelsInputAccess,
		localaccess_t gradientOutputAccess)
	{
		m_data[15] |= (5 << 4);
		m_data[3] = (numIterations << 16);
		m_data[4] = labelsInputAccess.GetReg();
		m_data[5] = ((useIndex0?1:0) << 3) | (algo << 2) | (type & 0x3);
		m_data[6] = *((uint32_t*)&stepSize);
		m_data[7] = *((uint32_t*)&lambda);
		m_data[8] = gradientOutputAccess.GetReg();
	}

	void Modify(
		uint32_t numIterations,
		uint32_t type, uint32_t algo, float stepSize, float lambda,
		localaccess_t labelsInputAccess,
		localaccess_t gradientOutputAccess)
	{
		bool useIndex0 = true;
		m_data[15] |= (5 << 4);
		m_data[3] = (numIterations << 16);
		m_data[4] = labelsInputAccess.GetReg();
		m_data[5] = ((useIndex0?1:0) << 3) | (algo << 2) | (type & 0x3);
		m_data[6] = *((uint32_t*)&stepSize);
		m_data[7] = *((uint32_t*)&lambda);
		m_data[8] = gradientOutputAccess.GetReg();
	}

	void Modify(
		uint32_t type, uint32_t algo, float stepSize, float lambda,
		localaccess_t labelsInputAccess,
		localaccess_t gradientOutputAccess)
	{
		uint32_t numIterations = 1;
		bool useIndex0 = true;
		m_data[15] |= (5 << 4);
		m_data[3] = (numIterations << 16);
		m_data[4] = labelsInputAccess.GetReg();
		m_data[5] = ((useIndex0?1:0) << 3) | (algo << 2) | (type & 0x3);
		m_data[6] = *((uint32_t*)&stepSize);
		m_data[7] = *((uint32_t*)&lambda);
		m_data[8] = gradientOutputAccess.GetReg();
	}

	void Update(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		localaccess_t samplesInputAccess,
		localaccess_t gradientInputAccess,
		localaccess_t modelReadAccess,
		localaccess_t modelWriteAccess,
		bool enableAsync)
	{
		m_data[15] |= (6 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = samplesInputAccess.GetReg();
		m_data[5] = gradientInputAccess.GetReg();
		m_data[6] = modelReadAccess.GetReg();
		m_data[7] = modelWriteAccess.GetReg();
		m_data[8] = enableAsync ? 1 : 0;
	}

	void Update(
		uint32_t numLinesToProcess,
		localaccess_t samplesInputAccess,
		localaccess_t gradientInputAccess,
		localaccess_t modelReadAccess,
		localaccess_t modelWriteAccess,
		bool enableAsync)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (6 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = samplesInputAccess.GetReg();
		m_data[5] = gradientInputAccess.GetReg();
		m_data[6] = modelReadAccess.GetReg();
		m_data[7] = modelWriteAccess.GetReg();
		m_data[8] = enableAsync ? 1 : 0;
	}

	void Update2(
		uint32_t numIterations,
		uint32_t numLinesToProcess,
		localaccess_t samplesInputAccess,
		localaccess_t gradientInputAccess,
		localaccess_t modelReadAccess,
		localaccess_t modelWriteAccess,
		bool enableAsync)
	{
		m_data[15] |= (10 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = samplesInputAccess.GetReg();
		m_data[5] = gradientInputAccess.GetReg();
		m_data[6] = modelReadAccess.GetReg();
		m_data[7] = modelWriteAccess.GetReg();
		m_data[8] = enableAsync ? 1 : 0;
	}

	void Update2(
		uint32_t numLinesToProcess,
		localaccess_t samplesInputAccess,
		localaccess_t gradientInputAccess,
		localaccess_t modelReadAccess,
		localaccess_t modelWriteAccess,
		bool enableAsync)
	{
		uint32_t numIterations = 1;
		m_data[15] |= (10 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = samplesInputAccess.GetReg();
		m_data[5] = gradientInputAccess.GetReg();
		m_data[6] = modelReadAccess.GetReg();
		m_data[7] = modelWriteAccess.GetReg();
		m_data[8] = enableAsync ? 1 : 0;
	}

	void L2Reg(
		uint32_t numLinesToProcess,
		localaccess_t modeloldInputAccess,
		localaccess_t modelnewInputAccess,
		localaccess_t modelforwardOutputAccess,
		localaccess_t modelnewOutputAccess,
		float lambda)
	{
		m_data[15] |= (12 << 4);
		m_data[3] = numLinesToProcess & 0xFFFF;
		m_data[4] = modeloldInputAccess.GetReg();
		m_data[5] = modelnewInputAccess.GetReg();
		m_data[6] = modelforwardOutputAccess.GetReg();
		m_data[7] = modelnewOutputAccess.GetReg();
		m_data[8] = *((uint32_t*)&lambda);
	}

	void Copy(
		localaccess_t sourceInputAccess,
		localaccess_t destinationOutputAccess)
	{
		m_data[15] |= (12 << 4);
		m_data[3] = sourceInputAccess.m_lengthInCL & 0xFFFF;
		m_data[4] = 0;
		m_data[5] = sourceInputAccess.GetReg();
		m_data[6] = destinationOutputAccess.GetReg();
		m_data[7] = 0;
		m_data[8] = 0;
	}

	void CopyPrefetch(
		localaccess_t sourceInputAccess,
		localaccess_t destinationOutputAccess,
		uint32_t channelSelection)
	{
		m_data[15] |= (7 << 4);
		m_data[3] = sourceInputAccess.GetReg();
		m_data[4] = destinationOutputAccess.GetReg();
		m_data[5] = channelSelection;
	}

	void WriteForward1(uint32_t numIterations, uint32_t numLinesToProcess) {
		m_data[15] |= (13 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
	}

	void WriteForward2(uint32_t numIterations, uint32_t numLinesToProcess) {
		m_data[15] |= (14 << 4);
		m_data[3] = (numIterations << 16) | (numLinesToProcess & 0xFFFF);
	}

	void LoadReg(
		uint32_t whichReg,
		uint32_t offsetInCL)
	{
		m_data[15] |= (11 << 4);
		m_data[3] = whichReg;
		m_data[4] = offsetInCL;
	}

	void Sync()
	{
		m_data[15] |= (8 << 4);
	}
};