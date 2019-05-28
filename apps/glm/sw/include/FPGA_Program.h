#pragma once

#include "ColumnML.h"
#include "iFPGA.h"
#include "Instruction.h"

class FPGA_Program {
protected:

	iFPGA* m_ifpga;
	volatile float* m_base = nullptr;
	bool m_useContextSwitch;
	bool m_inputHandleAllocated;

	Instruction m_inst[Instruction::MAX_NUM_INSTRUCTIONS];

	void WriteProgramMemory(uint32_t pcContextStore, uint32_t pcContextLoad) {
		cout << "WriteProgramMemory..." << endl;

		uint32_t pcStart = 0;
		auto output = iFPGA::CastToInt(m_outputHandle);
		output[0] = 0; // Done
		output[1] = 0; // reg 0;
		output[2] = 0; // reg 1;
		output[3] = 0; // reg 2;
		output[4] = ((pcContextLoad&0xFF) << 16) | ((pcContextStore&0xFF) << 8) | (pcStart&0xFF);
		cout << "m_numInstructions: " << m_numInstructions << endl;

		m_programSizeInCL = m_numInstructions*Instruction::NUM_BYTES/64;
		m_ifpga->Realloc(m_programMemoryHandle, m_numInstructions*Instruction::NUM_BYTES);
		auto programMemory = iFPGA::CastToInt(m_programMemoryHandle);
		assert(NULL != programMemory);
		for (uint32_t i = 0; i < m_numInstructions; i++) {
			m_inst[i].LoadInstruction(programMemory + i*Instruction::NUM_WORDS);
		}
	}

public:
	iFPGA_ptr m_inputHandle;
	iFPGA_ptr m_outputHandle;
	iFPGA_ptr m_programMemoryHandle;

	uint32_t m_numInstructions;

	uint32_t m_inputSizeInCL;
	uint32_t m_outputSizeInCL;
	uint32_t m_programSizeInCL;

	FPGA_Program(iFPGA* ifpga, bool useContextSwitch) {
		m_inputHandleAllocated = false;
		m_inputHandle = NULL;
		m_outputHandle = NULL;
		m_programMemoryHandle = NULL;
		m_ifpga = ifpga;
		m_useContextSwitch = useContextSwitch;
	}

	~FPGA_Program() {
		if (m_inputHandleAllocated) {
			m_ifpga->Free(m_inputHandle);
		}
		m_ifpga->Free(m_outputHandle);
		m_ifpga->Free(m_programMemoryHandle);
#ifdef XILINX
		m_ifpga->RemoveCopiedHandle(m_inputHandle, GetNumCLsAllocated());
#endif
	}

	uint32_t GetBank() {
		return m_ifpga->GetBank();
	}

	volatile float* GetBase() {return m_base;}

	uint32_t GetNumCLsAllocated() {
		return m_inputSizeInCL + m_outputSizeInCL + m_programSizeInCL;
	}

	void ResetContext() {
		auto output = iFPGA::CastToInt(m_outputHandle);
		output[0] = 0; // Done
		output[1] = 0; // reg 0;
		output[2] = 0; // reg 1;
		output[3] = 0; // reg 2;
	}

	static bool CheckMemoryFit(uint32_t requestedSize, uint32_t onchipSize, const char* name) {
		if (requestedSize > onchipSize) {
			cout << "requestedSize for memory " << name << " (" << requestedSize << ")";
			cout << " is larger than onchipSize (" << onchipSize << ")" << endl;
			return false;
		}
		else {
			return true;
		}
	}
};