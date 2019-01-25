#pragma once

#include "Unit.h"
#include <math.h>

class RemoteVector : public Unit{
public:
	RemoteVector(string name) : Unit(name, t_vector, t_remote) {}

	string LoadChannel(Unit* toWrite, unsigned channelNumber) {
		string channel("");
		if (toWrite->GetLocation() == t_local) {
			channel += "write_bram\n";
		}
		else {
			channel += "write_fifo\n";
		}
		channel += "write_" + toWrite->GetName() + "_inst (\n";
		channel += ".clk, .reset,\n";
		channel += ".op_start(op_start[1]),\n";
		channel += ".configreg(regs[" + to_string(5+channelNumber) + "]),\n";
		channel += ".into_write(from_load.commonwrite_source),\n";
		if (toWrite->GetLocation() == t_local) {
			channel += ".memory_access(" + toWrite->GetInterfaceName() + ".bram_write)\n";
		}
		else {
			channel += ".fifo_access(" + toWrite->GetInterfaceName() + ".fifo_write)\n";
		}
		channel += ");\n";
		return channel;
	}

	string StoreChannel(Unit* toRead, unsigned channelNumber) {
		string channel("");
		channel += "internal_interface #(.WIDTH(512)) to_writeback" + to_string(channelNumber) + "();\n";
		if (toRead->GetLocation() == t_local) {
			channel += "read_bram\n";
		}
		else {
			channel += "read_fifo\n";
		}
		channel += "read_" + toRead->GetName() + "_inst (\n";
		channel += ".clk, .reset,\n";
		channel += ".op_start(op_start[2]),\n";
		channel += ".configreg(regs[" + to_string(6+channelNumber) + "]),\n";
		if (toRead->GetLocation() == t_local) {
			channel += ".memory_access(" + toRead->GetInterfaceName() +  + ".bram_read),\n";
		}
		else {
			channel += ".fifo_access(" + toRead->GetInterfaceName() +  + ".fifo_read),\n";
		}
		channel += ".outfrom_read(to_writeback" + to_string(channelNumber) + ".commonread_source)\n";
		channel += ");\n";
		return channel;
	}

	string StoreChannelSelection(unsigned numChannels) {
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
			code += "\t\tto_writeback.rvalid = to_writeback" + to_string(i) + ".rvalid;\n";
			code += "\t\tto_writeback.rdata = to_writeback" + to_string(i) + ".rdata;\n";
			code += "\tend\n";
		}
		for (unsigned i = 0; i < numChannels; i++) {
			code += "\tto_writeback" + to_string(i) + ".almostfull = to_writeback.almostfull;\n";
		}
		code += "end\n";
		return code;
	}

	string Instantiate(string fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			unsigned loadChannelNumber = 0;
			for (Unit* output: m_outputs) {
				string channel = LoadChannel(output, loadChannelNumber);
				loadChannelNumber++;
				FindAndInstert(fileName, "//?LOADCH", channel);
				output->Instantiate(fileName);
			}

			if(m_name.compare("writeback") == 0) {
				unsigned storeChannelNumber = 0;
				for (Unit* input: m_leftInputs) {
					string channel = StoreChannel(input, storeChannelNumber);
					storeChannelNumber++;
					FindAndInstert(fileName, "//?STORECH", channel);
				}
				FindAndInstert(fileName, "//?STORECH", StoreChannelSelection(m_leftInputs.size()));
			}
			m_instantiated = true;
		}
	}
};

class LocalVector : public Unit{
private:
	unsigned m_size;
	unsigned m_log2size;

public:
	LocalVector(string name, unsigned size) : Unit(name, t_vector, t_local) {
		m_size = size;
		float temp = log2(size);
		m_log2size = (unsigned)(temp+0.5);
	}

	string Instantiate(string fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			string instantiation("");
			instantiation += "fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(" + to_string(m_log2size) + ")) " + m_interfaceName + "();\n";
			instantiation += "bram\n";
			instantiation += "#(.WIDTH(512), .LOG2_DEPTH(" + to_string(m_log2size) + "))\n";
			instantiation += m_name + "_inst (\n";
			instantiation += ".clk,\n";
			instantiation += ".access(" + m_interfaceName + ".bram_source)\n";
			instantiation += ");\n";
			FindAndInstert(fileName, "//?LOCALMEM", instantiation);

			for (Unit* output: m_outputs) {
				if (output->GetName().compare("writeback") != 0) {
					output->Instantiate(fileName);
				}
			}

			m_instantiated = true;
		}
	}
};

class TempVector : public Unit{
private:
	unsigned m_size;
	unsigned m_log2size;

public:
	TempVector(string name, unsigned size) : Unit(name, t_vector, t_temporary) {
		m_size = size;
		float temp = log2(size);
		m_log2size = (unsigned)(temp+0.5);
	}

	string Instantiate(string fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			string instantiation("");
			instantiation += "fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(" + to_string(m_log2size) + ")) " + m_interfaceName + "();\n";
			instantiation += "fifo\n";
			instantiation += "#(.WIDTH(512), .LOG2_DEPTH(" + to_string(m_log2size) + "))\n";
			instantiation += m_name + "_inst (\n";
			instantiation += ".clk, .reset,\n";
			instantiation += ".access(" + m_interfaceName + ".fifo_source)\n";
			instantiation += ");\n";
			FindAndInstert(fileName, "//?LOCALMEM", instantiation);

			for (Unit* output: m_outputs) {
				if (output->GetName().compare("writeback") != 0) {
					output->Instantiate(fileName);
				}
			}

			m_instantiated = true;
		}
	}
};

class LocalScalar : public Unit{
public:
	LocalScalar(string name) : Unit(name, t_scalar, t_local) {}
};

class TempScalar : public Unit{
public:
	TempScalar(string name) : Unit(name, t_scalar, t_temporary) {}
};