#pragma once

#include "PipeArch.h"

// class ComputeDot : public Compute {

// public:
// 	ComputeDot(unsigned opId, string name) : Compute(opId, 3, name, t_scalar) {}

// 	virtual void Instantiate() {
// 		if (!m_instantiated) {
// 			string fromName("from_" + m_name);
// 			string toName("to_" + m_name);
// 			string instantiation("");
// 			instantiation += "internal_interface #(.WIDTH(32)) " + fromName + "();\n";
// 			instantiation += "internal_interface #(.WIDTH(512)) " + toName + "_left();\n";
// 			instantiation += "internal_interface #(.WIDTH(512)) " + toName + "_right();\n";
// 			instantiation += "pipearch_dot\n";
// 			instantiation += "pipearch_dot_inst (\n";
// 			instantiation += ".clk, .reset,\n";
// 			instantiation += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
// 			instantiation += ".op_done(op_done[" + to_string(m_opId) + "]),\n";
// 			instantiation += ".regs0(regs[" + to_string(m_numDefaultRegs) + "]),\n";
// 			instantiation += ".left_input(" + toName + "_left.from_commonread),\n";
// 			instantiation += ".right_input(" + toName + "_right.from_commonread),\n";
// 			instantiation += ".result(" + fromName + ")\n";
// 			instantiation += ");\n";

// 			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", instantiation);

// 			Compute::Instantiate();

// 			string opcode("");
// 			opcode += "8'd" + to_string(10+m_opId) + ":\n";
// 			opcode += "begin\n";
// 			opcode += "\top_start[" + to_string(m_opId) + "] <= 1'b1;\n";
// 			opcode += "\tregs[3] <= instruction[3];\n";
// 			opcode += "\tregs[4] <= instruction[4];\n";
// 			opcode += "\tregs[5] <= instruction[5];\n";
// 			opcode += "\tprogram_counter <= program_counter + 1;\n";
// 			opcode += "end\n";
// 			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?OPCODES", opcode);

// 			string instruction("");
// 			instruction += "void Dot(\n";
// 			instruction += "\tuint32_t numLinesToProcess,\n";
// 			instruction += "\tuint32_t leftChannelSelect,\n";
// 			instruction += "\tuint32_t rightChannelSelect)\n";
// 			instruction += "{\n";
// 			instruction += "\tm_data[15] = " + to_string(10+m_opId) + ";\n";
// 			instruction += "\tm_data[3] = numLinesToProcess << 16;\n";
// 			instruction += "\tm_data[4] = ((rightChannelSelect & 0xF) << 4) | (leftChannelSelect & 0xF);\n";
// 			instruction += "\tm_data[5] = 1 << 16;\n";
// 			instruction += "}\n";

// 			FileOps::FindAndInstert(FileOps::m_instructionsHeaderFileName, "//?INST", instruction);

// 			m_instantiated = true;

// 			for (Unit* output: m_outputs) {
// 				output->Instantiate();
// 			}
// 		}
// 	}
// };

// class ComputeScalarSubtract : public Compute {

// public:
// 	ComputeScalarSubtract(unsigned opId, string name) : Compute(opId, 3, name, t_scalar) {}

// 	virtual void Instantiate() {
// 		if (!m_instantiated) {
// 			string fromName("from_" + m_name);
// 			string toName("to_" + m_name);
// 			string instantiation("");
// 			instantiation += "internal_interface #(.WIDTH(32)) " + fromName + "();\n";
// 			instantiation += "internal_interface #(.WIDTH(32)) " + toName + "_left();\n";
// 			instantiation += "internal_interface #(.WIDTH(32)) " + toName + "_right();\n";
// 			instantiation += "pipearch_scalar_subtract\n";
// 			instantiation += "pipearch_scalar_subtract_inst (\n";
// 			instantiation += ".clk, .reset,\n";
// 			instantiation += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
// 			instantiation += ".op_done(op_done[" + to_string(m_opId) + "]),\n";
// 			instantiation += ".regs0(regs[" + to_string(m_numDefaultRegs) + "]),\n";
// 			instantiation += ".left_input(" + toName + "_left.from_commonread),\n";
// 			instantiation += ".right_input(" + toName + "_right.from_commonread),\n";
// 			instantiation += ".result(" + fromName + ")\n";
// 			instantiation += ");\n";

// 			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", instantiation);

// 			Compute::Instantiate();

// 			string opcode("");
// 			opcode += "8'd" + to_string(10+m_opId) + ":\n";
// 			opcode += "begin\n";
// 			opcode += "\top_start[" + to_string(m_opId) + "] <= 1'b1;\n";
// 			opcode += "\tregs[3] <= instruction[3];\n";
// 			opcode += "\tregs[4] <= instruction[4];\n";
// 			opcode += "\tregs[5] <= instruction[5];\n";
// 			opcode += "\tprogram_counter <= program_counter + 1;\n";
// 			opcode += "end\n";
// 			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?OPCODES", opcode);

// 			string instruction("");
// 			instruction += "void ScalarSubtract(\n";
// 			instruction += "\tuint32_t numLinesToProcess)\n";
// 			instruction += "{\n";
// 			instruction += "\tm_data[15] = " + to_string(10+m_opId) + ";\n";
// 			instruction += "\tm_data[3] = numLinesToProcess << 16;\n";
// 			instruction += "\tm_data[4] = 0;\n";
// 			instruction += "\tm_data[5] = 1 << 16;\n";
// 			instruction += "}\n";

// 			FileOps::FindAndInstert(FileOps::m_instructionsHeaderFileName, "//?INST", instruction);

// 			m_instantiated = true;

// 			for (Unit* output: m_outputs) {
// 				output->Instantiate();
// 			}
// 		}
// 	}
// };


class PipeArchBase : public PipeArch {
public:

	void Load(Unit& result) {
		DirectLink(&result, m_load);
	}

	void WriteBack(Unit& source) {
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
		Compute* compute = new Compute(m_numOps, "Dot_" + result.GetName(), t_scalar, "pipearch_dot", 3, 3, 3, 4, 5);
		LeftRightLink(compute, &left, &right);
		DirectLink(&result, compute);
	}

	void ScalarSubtract(Unit& result, Unit& left, Unit& right) {
		Unit::CheckScalar(result, "ScalarSubtract");
		Unit::CheckScalar(left, "ScalarSubtract");
		Unit::CheckScalar(right, "ScalarSubtract");

		m_numOps++;
		Compute* compute = new Compute(m_numOps, "ScalarSubtract_" + result.GetName(), t_scalar, "pipearch_scalar_subtract", 3, 3, 4, 5, 3);
		LeftRightLink(compute, &left, &right);
		DirectLink(&result, compute);
	}
};