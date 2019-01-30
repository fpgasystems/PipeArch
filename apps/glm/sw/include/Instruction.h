#pragma once

struct access_t {
	uint32_t m_offsetInCL;
	uint32_t m_lengthInCL;
};

class Instruction {
public:
	static const uint32_t NUM_WORDS = 16;
	static const uint32_t NUM_BYTES = NUM_WORDS*4;
	uint32_t m_data[NUM_WORDS];

	Instruction() {
		for (unsigned i = 0; i < 16; i++) {
			m_data[i] = 0;
		}
		// Maintain indexes by default
		m_data[0] = 0xFFFFFFFF;
		m_data[1] = 0xFFFFFFFF;
		m_data[2] = 0xFFFFFFFF;
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

	void DecrementIndex(uint32_t whichIndex) {
		m_data[whichIndex] = 0x1FFFFFF;
	}

	void MakeNonBlocking() {
		m_data[15] |= (1 << 8);
	}

	void Jump(
		uint32_t whichReg,
		uint32_t predicate,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] = whichReg; // opcode
		m_data[12] = predicate;
		m_data[14] = nextPCFalse;
		m_data[13] = nextPCTrue;
	}

	void Prefetch(
		uint32_t loadOffsetDRAM,
		uint32_t loadLengthDRAM,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2)
	{
		m_data[15] = 10;
		m_data[3] = loadOffsetDRAM;
		m_data[4] = loadLengthDRAM;
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
		access_t* accessProperties,
		uint32_t numLoadChannels)
	{
		m_data[15] = 11;
		m_data[3] = loadOffsetDRAM;
		m_data[4] = loadLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		for (uint32_t i = 0; i < numLoadChannels; i++) {
			m_data[5+i] = (accessProperties[i].m_lengthInCL << 16) | accessProperties[i].m_offsetInCL;
		}
	}

	void WriteBack(
		uint32_t storeOffsetDRAM,
		uint32_t storeLengthDRAM,
		uint32_t offsetByIndex0,
		uint32_t offsetByIndex1,
		uint32_t offsetByIndex2,
		uint32_t whichChannel,
		access_t* accessProperties,
		uint32_t numWriteBackChannels)
	{
		m_data[15] = 12;
		m_data[3] = storeOffsetDRAM;
		m_data[4] = storeLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		m_data[5] = whichChannel;
		for (uint32_t i = 0; i < numWriteBackChannels; i++) {
			m_data[6+i] = (accessProperties[i].m_lengthInCL << 16) | accessProperties[i].m_offsetInCL;
		}
	}

// *************************************************************************
//   FILL: Instructions
// *************************************************************************
//?INST

	void Copy(volatile uint32_t* data) {
		for (uint32_t i = 0; i < NUM_WORDS; i++) {
			data[i] = m_data[i];
		}
	}
};