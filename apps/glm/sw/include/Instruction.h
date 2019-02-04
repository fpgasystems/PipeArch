#pragma once

struct access_t {
	uint32_t m_offsetInCL;
	uint32_t m_lengthInCL;
};

class AccessProperties {
private:
	access_t* m_properties;
	uint32_t m_numChannels;

public:
	AccessProperties(uint32_t numChannels)  {
		m_numChannels = numChannels;
		m_properties = new access_t[numChannels];
		for (uint32_t i = 0; i < numChannels; i++) {
			m_properties[i].m_offsetInCL = 0;
			m_properties[i].m_lengthInCL = 0;
		}
	}

	// ~AccessProperties() {
	// 	delete[] m_properties;
	// }

	void Set(uint32_t channel, uint32_t offsetInCL, uint32_t lengthInCL) {
		m_properties[channel].m_offsetInCL = offsetInCL;
		m_properties[channel].m_lengthInCL = lengthInCL;
	}

	access_t Get(uint32_t channel) {
		return m_properties[channel];
	}

	uint32_t GetNumChannels() {
		return m_numChannels;
	}
};

class Instruction {
public:
	static const uint32_t MAX_NUM_INSTRUCTIONS = 32;
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

	void Copy(volatile uint32_t* data) {
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

	void DecrementIndex(uint32_t whichIndex) {
		m_data[whichIndex] = 0x1FFFFFF;
	}

	void MakeNonBlocking() {
		m_data[15] |= (1 << 8);
	}

	void AddJump(
		uint32_t whichReg,
		uint32_t predicate,
		uint32_t nextPCFalse,
		uint32_t nextPCTrue)
	{
		m_data[15] |= (whichReg+1); // opcode
		m_data[13] = predicate;
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
		AccessProperties accessProperties)
	{
		uint32_t enableMultiline = 1;
		m_data[15] |= (2 << 4);
		m_data[3] = loadOffsetDRAM;
		m_data[4] = (enableMultiline << 31) | loadLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		for (uint32_t i = 0; i < accessProperties.GetNumChannels(); i++) {
			m_data[5+i] = (accessProperties.Get(i).m_lengthInCL << 16) | accessProperties.Get(i).m_offsetInCL;
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
		AccessProperties accessProperties)
	{
		m_data[15] |= (3 << 4);
		m_data[3] = ((uint32_t)useInputSpace << 31) | storeOffsetDRAM;
		m_data[4] = storeLengthDRAM;
		m_data[10] = offsetByIndex0;
		m_data[11] = offsetByIndex1;
		m_data[12] = offsetByIndex2;
		m_data[5] = (((uint32_t)writeFence << 4) | (whichChannel & 0xFFFF));
		for (uint32_t i = 0; i < accessProperties.GetNumChannels(); i++) {
			m_data[6+i] = (accessProperties.Get(i).m_lengthInCL << 16) | accessProperties.Get(i).m_offsetInCL;
		}
	}

	// *************************************************************************
	//
	//   Instructions
	//
	// *************************************************************************

	void Dot(
		uint32_t numLinesToProcess,
		bool readFromModelForward,
		bool performSubtraction,
		uint32_t memModelLoadOffset,
		uint32_t memLabelsLoadOffset)
	{
		m_data[15] |= (4 << 4);
		m_data[3] = (((uint32_t)performSubtraction) << 17) | (((uint32_t)readFromModelForward) << 16) | (numLinesToProcess & 0xFFFF);
		m_data[4] = ((memLabelsLoadOffset & 0xFFFF) << 16) | (memModelLoadOffset & 0xFFFF);
	}

	void Modify(
		uint32_t memLabelsLoadOffset,
		uint32_t type,
		uint32_t algo,
		float stepSize,
		float lambda)
	{
		m_data[15] |= (5 << 4);
		m_data[3] = (memLabelsLoadOffset & 0xFFFF);
		m_data[4] = (algo << 2) | (type & 0x3);
		m_data[5] = *((uint32_t*)&stepSize);
		m_data[6] = *((uint32_t*)&lambda);
	}

	void Update(
		uint32_t memModelLoadOffset,
		uint32_t loadLength,
		bool modelForward)
	{
		m_data[15] |= (6 << 4);
		m_data[3] = (loadLength << 16) | (memModelLoadOffset & 0xFFFF);
		m_data[4] = (uint32_t)modelForward;
	}

};