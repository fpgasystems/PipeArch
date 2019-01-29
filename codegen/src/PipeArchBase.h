#pragma once

#include "PipeArch.h"

class ComputeDot : public Compute {

public:
	ComputeDot(unsigned opId, string name) : Compute(opId, 3, name, t_scalar) {}

	virtual void Instantiate() {
		if (!m_instantiated) {
			string fromName("from_" + m_name);
			string toName("to_" + m_name);
			string instantiation("");
			instantiation += "internal_interface #(.WIDTH(32)) " + fromName + "();\n";
			instantiation += "internal_interface #(.WIDTH(512)) " + toName + "_left();\n";
			instantiation += "internal_interface #(.WIDTH(512)) " + toName + "_right();\n";
			instantiation += "pipearch_dot\n";
			instantiation += "pipearch_dot_inst (\n";
			instantiation += ".clk, .reset,\n";
			instantiation += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
			instantiation += ".op_done(op_done[" + to_string(m_opId) + "]),\n";
			instantiation += ".regs0(regs[" + to_string(m_numDefaultRegs) + "]),\n";
			instantiation += ".left_input(" + toName + "_left.from_commonread),\n";
			instantiation += ".right_input(" + toName + "_right.from_commonread),\n";
			instantiation += ".result(" + fromName + ")\n";
			instantiation += ");\n";

			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", instantiation);

			unsigned readChannelNumber = 0;
			for (Unit* input: m_leftInputs) {
				string channel = ReadChannel(input, 3);
				readChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", channel);
			}
			for (Unit* input: m_rightInputs) {
				string channel = ReadChannel(input, 3);
				readChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", channel);
			}
			unsigned writeChannelNumber = 0;
			for (Unit* output: m_outputs) {
				string channel = WriteChannel(output, 5);
				writeChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", channel);
				output->Instantiate();
			}

			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", ReadChannelSelection("to_" + m_name + "_left", m_leftInputs, 4, 0));
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", ReadChannelSelection("to_" + m_name + "_right", m_rightInputs, 4, 4));
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", WriteChannelSelection(m_outputs));


			string opcode("");
			opcode += "8'd" + to_string(10+m_opId) + ":\n";
			opcode += "begin\n";
			opcode += "\top_start[" + to_string(m_opId) + "] <= 1'b1;\n";
			opcode += "\tregs[3] <= instruction[3];\n";
			opcode += "\tregs[4] <= instruction[4];\n";
			opcode += "\tregs[5] <= instruction[5];\n";
			opcode += "\tprogram_counter <= program_counter + 1;\n";
			opcode += "end\n";
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?OPCODES", opcode);

			string instruction("");
			instruction += "void Dot(\n";
			instruction += "\tuint32_t numLinesToProcess,\n";
			instruction += "\tuint32_t leftChannelSelect,\n";
			instruction += "\tuint32_t rightChannelSelect)\n";
			instruction += "{\n";
			instruction += "\tm_data[15] = " + to_string(10+m_opId) + ";\n";
			instruction += "\tm_data[3] = numLinesToProcess << 16;\n";
			instruction += "\tm_data[4] = ((rightChannelSelect & 0xF) << 4) | (leftChannelSelect & 0xF);\n";
			instruction += "\tm_data[5] = 1 << 16;\n";
			instruction += "}\n";

			FileOps::FindAndInstert(FileOps::m_instructionsHeaderFileName, "//?INST", instruction);

			m_instantiated = true;
		}
	}
};

class PipeArchBase : public PipeArch {
public:

	void Load(Unit& result) {
		DirectLink(&result, m_load);
	}

	void WriteBack(Unit& source) {
		// if (source.GetOutputWidth() != t_vector) {
		// 	cout << "WriteBack, source output is not a vector" << endl;
		// 	exit(1);
		// }
		if (source.GetUnitType() == t_remote) {
			cout << "WriteBack, source cannot be remote. Either temporary or local." << endl;
			exit(1);
		}
		DirectLink(m_writeback, &source);
	}

	void Copy(Unit& destination, Unit& source) {
		if (destination.GetOutputWidth() != source.GetOutputWidth()) {
			cout << "Copy, source output width does not match destination" << endl;
			exit(1);
		}
		DirectLink(&destination, &source);
	}

	void Dot(Unit& result, Unit& left, Unit& right) {
		Unit::CheckScalar(result, "Dot");
		Unit::CheckVector(left, "Dot");
		Unit::CheckVector(right, "Dot");

		m_numOps++;
		Compute* compute = new ComputeDot(m_numOps, "Dot" + result.GetName());
		LeftRightLink(compute, &left, &right);
		DirectLink(&result, compute);
	}
};