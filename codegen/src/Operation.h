#include "Unit.h"

class Operation : public Unit {
private:
	unsigned m_opId;
	unsigned m_numDefaultRegs;

public:
	Operation(
		unsigned opId,
		unsigned numDefaultRegs,
		string name,
		OutputWidth outputWidth,
		UnitType unitType)
	: Unit(name, outputWidth, unitType)
	{
		m_opId = opId;
		m_numDefaultRegs = numDefaultRegs;
	}

	string WriteChannel(Unit* toWrite, unsigned channelNumber) {
		string channel("");
		if (toWrite->GetUnitType() == t_local) {
			channel += "write_bram\n";
		}
		else {
			channel += "write_fifo\n";
		}
		channel += "write_" + toWrite->GetName() + "_inst (\n";
		channel += ".clk, .reset,\n";
		channel += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
		channel += ".configreg(regs[" + to_string(m_numDefaultRegs+channelNumber) + "]),\n";
		channel += ".into_write(from_" + m_name + ".commonwrite_source),\n";
		if (toWrite->GetUnitType() == t_local) {
			channel += ".memory_access(" + toWrite->GetInterfaceName() + ".bram_write)\n";
		}
		else {
			channel += ".fifo_access(" + toWrite->GetInterfaceName() + ".fifo_write)\n";
		}
		channel += ");\n";
		return channel;
	}

	string ReadChannel(Unit* toRead, unsigned channelNumber) {
		string channel("");
		channel += "internal_interface #(.WIDTH(512)) to_" + m_name + to_string(channelNumber) + "();\n";
		if (toRead->GetUnitType() == t_local) {
			channel += "read_bram\n";
		}
		else {
			channel += "read_fifo\n";
		}
		channel += "read_" + toRead->GetName() + "_inst (\n";
		channel += ".clk, .reset,\n";
		channel += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
		channel += ".configreg(regs[" + to_string(m_numDefaultRegs+channelNumber) + "]),\n";
		if (toRead->GetUnitType() == t_local) {
			channel += ".memory_access(" + toRead->GetInterfaceName() +  + ".bram_read),\n";
		}
		else {
			channel += ".fifo_access(" + toRead->GetInterfaceName() +  + ".fifo_read),\n";
		}
		channel += ".outfrom_read(to_" + m_name + to_string(channelNumber) + ".commonread_source)\n";
		channel += ");\n";
		return channel;
	}

	string ReadChannelSelection(unsigned numChannels) {
		string code("");
		code += "always_comb\n";
		code += "begin\n";
		for (unsigned i = 0; i < numChannels; i++) {
			if (i == 0) {
				code += "\tif ";
			}
			else {
				code += "\telse if ";
			}
			code += "(regs[5] == " + to_string(i) + ") begin\n";
			code += "\t\tto_" + m_name + ".rvalid = to_" + m_name + to_string(i) + ".rvalid;\n";
			code += "\t\tto_" + m_name + ".rdata = to_" + m_name + to_string(i) + ".rdata;\n";
			code += "\tend\n";
		}
		for (unsigned i = 0; i < numChannels; i++) {
			code += "\tto_" + m_name + to_string(i) + ".almostfull = to_" + m_name + ".almostfull;\n";
		}
		code += "end\n";
		return code;
	}
};