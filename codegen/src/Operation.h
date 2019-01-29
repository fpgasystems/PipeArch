#include "Unit.h"

class Operation : public Unit {
protected:
	unsigned m_opId;

public:
	Operation(
		unsigned opId,
		string name,
		OutputWidth outputWidth,
		UnitType unitType)
	: Unit(name, outputWidth, unitType)
	{
		m_opId = opId;
	}

	string WriteChannel(Unit* toWrite, unsigned regNumber) {
		string channel("");
		string fromName("from_" + m_name + "_to_" + toWrite->GetName());
		if (m_outputWidth == t_scalar) {
			channel += "internal_interface #(.WIDTH(32)) " + fromName + "();\n";
		}
		else {
			channel += "internal_interface #(.WIDTH(512)) " + fromName + "();\n";
		}
		if (toWrite->GetUnitType() == t_local) {
			if (m_outputWidth == t_vector && toWrite->GetOutputWidth() == t_scalar) {
				channel += "write_bram_vector2scalar\n";
			}
			else {
				channel += "write_bram\n";
			}
		}
		else {
			channel += "write_fifo\n";
		}
		channel += "write_" + toWrite->GetName() + "_inst (\n";
		channel += ".clk, .reset,\n";
		channel += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
		channel += ".configreg(regs[" + to_string(regNumber) + "]),\n";
		channel += ".into_write(" + fromName + ".commonwrite_source),\n";
		if (toWrite->GetUnitType() == t_local) {
			channel += ".memory_access(" + toWrite->GetInterfaceName() + ".bram_write)\n";
		}
		else {
			channel += ".fifo_access(" + toWrite->GetInterfaceName() + ".fifo_write)\n";
		}
		channel += ");\n";
		return channel;
	}

	string WriteChannelSelection(list<Unit*>& outputList) {

		string fromName("from_" + m_name);
		string code("");
		code += "always_comb\n";
		code += "begin\n";
		
		for (Unit* output : outputList) {
			code += "\t" + fromName + "_to_" + output->GetName() + ".we = " + fromName + ".we;\n";
			code += "\t" + fromName + "_to_" + output->GetName() + ".wdata = " + fromName + ".wdata;\n";
		}
		code += "end\n";

		code += "assign " + fromName + ".almostfull = ";
		unsigned i = 0;
		for (Unit* output : outputList) {
			
			if (i == outputList.size()-1) {
				code += fromName + "_to_" + output->GetName() + ".almostfull;\n";
			}
			else {
				code += fromName + "_to_" + output->GetName() + ".almostfull | ";
			}
			i++;
		}

		return code;
	}

	string ReadChannel(Unit* toRead, unsigned regNumber) {
		string channel("");
		string toName("to_" + m_name + "_from_" + toRead->GetName());
		if (toRead->GetOutputWidth() == t_scalar) {
			channel += "internal_interface #(.WIDTH(32)) " + toName + "();\n";
		}
		else {
			channel += "internal_interface #(.WIDTH(512)) " + toName + "();\n";
		}
		if (toRead->GetUnitType() == t_local) {
			channel += "read_bram\n";
		}
		else {
			channel += "read_fifo\n";
		}
		channel += "read_" + toRead->GetName() + "_inst (\n";
		channel += ".clk, .reset,\n";
		channel += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
		channel += ".configreg(regs[" + to_string(regNumber) + "]),\n";
		if (toRead->GetUnitType() == t_local) {
			channel += ".memory_access(" + toRead->GetInterfaceName() +  + ".bram_read),\n";
		}
		else {
			channel += ".fifo_access(" + toRead->GetInterfaceName() +  + ".fifo_read),\n";
		}
		channel += ".outfrom_read(" + toName + ".commonread_source)\n";
		channel += ");\n";
		return channel;
	}

	string ReadChannelSelection(string toName, list<Unit*>& inputsList, unsigned selectionRegister, unsigned selectionRegisterRange) {
		string code("");
		code += "always_comb\n";
		code += "begin\n";

		unsigned i = 0;
		for (Unit* input : inputsList) {
			if (i == 0) {
				code += "\tif ";
			}
			else {
				code += "\telse if ";
			}
			code += "(regs[" + to_string(selectionRegister) + "][" +
					to_string(selectionRegisterRange+3) + ":" +
					to_string(selectionRegisterRange) + "] == " + to_string(i) + ") begin\n";
			code += "\t\t" + toName + ".rvalid = to_" + m_name + "_from_" + input->GetName() + ".rvalid;\n";
			code += "\t\t" + toName + ".rdata = to_" + m_name + "_from_" + input->GetName() + ".rdata;\n";
			code += "\t\tto_" + m_name + "_from_" + input->GetName() + ".almostfull = " + toName + ".almostfull;\n";
			code += "\tend\n";
			i++;
		}
		code += "end\n";

		return code;
	}
};