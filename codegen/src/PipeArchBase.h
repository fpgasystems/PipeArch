#pragma once

#include "PipeArch.h"

class PipeArchBase : public PipeArch {

public:

	void Load(Unit& result) {
		if (result.GetLocation() == t_remote) {
			for (Unit* u: result.GetOutputs()) {
				m_load->AddOutput(u);
			}
		}
		else if (result.GetLocation() == t_local) {
			DirectLink(&result, m_load);
		}
	}

	void WriteBack(Unit& source) {
		if (source.GetOutputWidth() != t_vector) {
			cout << "WriteBack, source output is not a vector" << endl;
			exit(1);
		}
		if (source.GetLocation() == t_remote) {
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
		if (left.GetOutputWidth() != t_vector) {
			cout << "Dot, left input is not a vector" << endl;
			exit(1);
		}
		if (right.GetOutputWidth() != t_vector) {
			cout << "Dot, right input is not a vector" << endl;
			exit(1);
		}
		if (result.GetOutputWidth() != t_scalar) {
			cout << "Dot, output is not a scalar" << endl;
			exit(1);
		}
		result.AddCompute(new Compute("Dot"));
		LeftRightLink(&result, &left, &right);
	}

	void ScalarSubtract(Unit& result, Unit& left, Unit& right) {
		if (left.GetOutputWidth() != t_scalar) {
			cout << "ScalarSubtract, left input is not a scalar" << endl;
			exit(1);
		}
		if (right.GetOutputWidth() != t_scalar) {
			cout << "ScalarSubtract, right input is not a scalar" << endl;
			exit(1);
		}
		if (result.GetOutputWidth() != t_scalar) {
			cout << "ScalarSubtract, output is not a scalar" << endl;
			exit(1);
		}
		result.AddCompute(new Compute("ScalarSubtract"));
		LeftRightLink(&result, &left, &right);
	}

	void ScalarVectorMultiply(Unit& result, Unit& left, Unit& right) {
		if (left.GetOutputWidth() != t_scalar) {
			cout << "ScalarVectorMultiply, left input is not a scalar" << endl;
			exit(1);
		}
		if (right.GetOutputWidth() != t_vector) {
			cout << "ScalarVectorMultiply, right input is not a vector" << endl;
			exit(1);
		}
		if (result.GetOutputWidth() != t_vector) {
			cout << "ScalarVectorMultiply, output is not a vector" << endl;
			exit(1);
		}
		result.AddCompute(new Compute("ScalarVectorMultiply"));
		LeftRightLink(&result, &left, &right);
	}

	void VectorSubtract(Unit& result, Unit& left, Unit& right) {
		if (left.GetOutputWidth() != t_vector) {
			cout << "VectorSubtract, left input is not a vector" << endl;
			exit(1);
		}
		if (right.GetOutputWidth() != t_vector) {
			cout << "VectorSubtract, right input is not a vector" << endl;
			exit(1);
		}
		if (result.GetOutputWidth() != t_vector) {
			cout << "VectorSubtract, output is not a vector" << endl;
			exit(1);
		}
		result.AddCompute(new Compute("VectorSubtract"));
		LeftRightLink(&result, &left, &right);
	}

};