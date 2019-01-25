#pragma once

#include "PipeArch.h"

class PipeArchBase : public PipeArch {

public:

	void Load(Unit& result) {
		// if (result.GetUnitType() == t_remote) {
		// 	for (Unit* u: result.GetOutputs()) {
		// 		m_load->AddOutput(u);
		// 	}
		// }
		// else if (result.GetUnitType() == t_local) {
		// 	DirectLink(&result, m_load);
		// }
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
		Compute* compute = new Compute(m_numOps, 4, "Dot_" + result.GetName(), t_scalar);
		LeftRightLink(compute, &left, &right);
		DirectLink(&result, compute);
	}


};