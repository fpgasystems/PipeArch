#pragma once

#include "PipeArch.h"

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

// START Experimental
	// void Dot(Unit& result, Unit& left, Unit& right) {
	// 	Unit::CheckScalar(result, "Dot");
	// 	Unit::CheckVector(left, "Dot");
	// 	Unit::CheckVector(right, "Dot");

	// 	m_numOps++;
	// 	Compute* compute = new Compute(m_numOps, "Dot_" + result.GetName(), t_scalar, "pipearch_dot", 3, 3, 3, 4, 5);
	// 	LeftRightLink(compute, &left, &right);
	// 	DirectLink(&result, compute);
	// }

	// void ScalarSubtract(Unit& result, Unit& left, Unit& right) {
	// 	Unit::CheckScalar(result, "ScalarSubtract");
	// 	Unit::CheckScalar(left, "ScalarSubtract");
	// 	Unit::CheckScalar(right, "ScalarSubtract");

	// 	m_numOps++;
	// 	Compute* compute = new Compute(m_numOps, "ScalarSubtract_" + result.GetName(), t_scalar, "pipearch_scalar_subtract", 3, 3, 4, 5, 3);
	// 	LeftRightLink(compute, &left, &right);
	// 	DirectLink(&result, compute);
	// }
// END Experimental
};