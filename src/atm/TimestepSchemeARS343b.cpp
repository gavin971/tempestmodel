///////////////////////////////////////////////////////////////////////////////
///
///	\file    TimestepSchemeARS343b.h
///	\author  Jorge Guerra
///	\version January 27, 2016
///
///	<remarks>
///		Copyright 2000-2010 Paul Ullrich, Jorge Guerra
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#include "TimestepSchemeARS343b.h"
#include "Model.h"
#include "Grid.h"
#include "HorizontalDynamics.h"
#include "VerticalDynamics.h"

///////////////////////////////////////////////////////////////////////////////

TimestepSchemeARS343b::TimestepSchemeARS343b(
	Model & model
) :
	TimestepScheme(model)
{
	m_iU2fCombo.Allocate(3);
	m_dU2fCombo.Allocate(3);

	m_iU3fCombo.Allocate(5);
	m_dU3fCombo.Allocate(5);

	m_iU4fCombo.Allocate(7);
	m_dU4fCombo.Allocate(7);

	m_dDiagExpCf.Allocate(4);
	m_dDiagImpCf.Allocate(4);

	// Initialize linear combinations corresponding to direct copies
	for (int s = 0; s < 7; s++) {
		m_iCopyInst[s].Allocate(1);
		m_iCopyInst[s][0] = s;

		m_dCopyInst[s].Allocate(1);
		m_dCopyInst[s][0] = 1.0;
	}

	///////////////////////////////////////////////////////////////////////////////
	// COEFFICIENTS COMPUTED FROM THE ORIGINAL TABLEAUX
	// IMPLEMENTS ARS(3,4,3) FROM ASCHER ET AL. 1997 PG. 9
	//
	const double dGamma = 0.4358665215084590;
 
	const double dB1 = -1.5 * dGamma * dGamma + 4.0 * dGamma - 0.25;
	const double dB2 = 1.5 * dGamma * dGamma - 5.0 * dGamma + 1.25;
 
	const double dA42 = 0.5529291480359398;
	const double dA43 = 0.5529291480359398;
 
	const double dA31 =
		(1.0 - 4.5 * dGamma + 1.5 * dGamma * dGamma) * dA42
			+ (2.75 - 10.5 * dGamma + 3.75 * dGamma * dGamma) * dA43
 	 		- 3.5 + 13 * dGamma - 4.5 * dGamma * dGamma;
 
	const double dA32 =
		(-1.0 + 4.5 * dGamma - 1.5 * dGamma * dGamma) * dA42
			+ (-2.75 + 10.5 * dGamma - 3.75 * dGamma * dGamma) * dA43
			+ 4.0 - 12.5 * dGamma + 4.5 * dGamma * dGamma;
 
	const double dA41 = 1.0 - dA42 - dA43;
 
	// Implicit stage coefficients
	const double dImpCf[4][4] = {
                {dGamma, 0., 0., 0.},
                {0.5 * (1.0 - dGamma), dGamma, 0., 0.},
                {dB1, dB2, dGamma, 0.},
                {dB1, dB2, dGamma, 0.}};
 
	// Explicit stage coefficients
	const double dExpCf[4][4] = {
                {dGamma, 0., 0., 0.},
                {dA31, dA32, 0., 0.},
                {dA41, dA42, dA43, 0.},
                {0., dB1, dB2, dGamma}};

	// Diagnoal explicit coefficients
	m_dDiagExpCf[0] = dExpCf[0][0];
	m_dDiagExpCf[1] = dExpCf[1][1];
	m_dDiagExpCf[2] = dExpCf[2][2];
	m_dDiagExpCf[3] = dExpCf[3][3];

	// Diagnoal implicit coefficients
	m_dDiagImpCf[0] = dImpCf[0][0];
	m_dDiagImpCf[1] = dImpCf[1][1];
	m_dDiagImpCf[2] = dImpCf[2][2];
	m_dDiagImpCf[3] = dImpCf[3][3];

	// u2 explicit evaluation combination
	m_iU2fCombo[0] = 0;
	m_iU2fCombo[1] = 1;
	m_iU2fCombo[2] = 2;

	m_dU2fCombo[0] = 1.0 - dExpCf[1][0] / dExpCf[0][0];
	m_dU2fCombo[1] = dExpCf[1][0] / dExpCf[0][0] - 
					 dImpCf[1][0] / dImpCf[0][0];
	m_dU2fCombo[2] = dImpCf[1][0] / dImpCf[0][0];

	// u3 explicit evaluation combination
	m_iU3fCombo[0] = 0;
	m_iU3fCombo[1] = 1;
	m_iU3fCombo[2] = 2;
	m_iU3fCombo[3] = 3;
	m_iU3fCombo[4] = 4;

	m_dU3fCombo[0] = 1.0 - dExpCf[2][0] / dExpCf[0][0];
	m_dU3fCombo[1] = dExpCf[2][0] / dExpCf[0][0] - 
					 dImpCf[2][0] / dImpCf[0][0];
	m_dU3fCombo[2] = dImpCf[2][0] / dImpCf[0][0];
	m_dU3fCombo[3] = dExpCf[2][1] / dExpCf[1][1] - 
					 dImpCf[2][1] / dImpCf[1][1];
	m_dU3fCombo[4] = dImpCf[2][1] / dImpCf[1][1];

	const double dU3fCombo7 = -dExpCf[2][1] / dExpCf[1][1];
	
	// u4 explicit evaluation combination
	m_iU4fCombo[0] = 0;
	m_iU4fCombo[1] = 1;
	m_iU4fCombo[2] = 2;
	m_iU4fCombo[3] = 3;
	m_iU4fCombo[4] = 4;
	m_iU4fCombo[5] = 5;
	m_iU4fCombo[6] = 6;

	m_dU4fCombo[0] = 1.0 - dExpCf[3][0] / dExpCf[0][0];
	m_dU4fCombo[1] = dExpCf[3][0] / dExpCf[0][0] - 
					 dImpCf[3][0] / dImpCf[0][0];
	m_dU4fCombo[2] = dImpCf[3][0] / dImpCf[0][0];
	m_dU4fCombo[3] = dExpCf[3][1] / dExpCf[1][1] - 
					 dImpCf[3][1] / dImpCf[1][1];
	m_dU4fCombo[4] = dImpCf[3][1] / dImpCf[1][1];
	m_dU4fCombo[5] = dExpCf[3][2] / dExpCf[2][2] - 
					 dImpCf[3][2] / dImpCf[2][2];
	m_dU4fCombo[6] = dImpCf[3][2] / dImpCf[2][2];

	const double dU4fCombo7 = -dExpCf[3][1] / dExpCf[1][1];
	const double dU4fCombo8 = -dExpCf[3][2] / dExpCf[2][2];

	// Recombination terms
	m_dU3fCombo[0] += dU3fCombo7 * m_dU2fCombo[0];
	m_dU3fCombo[1] += dU3fCombo7 * m_dU2fCombo[1];
	m_dU3fCombo[2] += dU3fCombo7 * m_dU2fCombo[2];

	m_dU4fCombo[0] +=
		  dU4fCombo7 * m_dU2fCombo[0]
		+ dU4fCombo8 * m_dU3fCombo[0];
	m_dU4fCombo[1] +=
		  dU4fCombo7 * m_dU2fCombo[1]
		+ dU4fCombo8 * m_dU3fCombo[1];
	m_dU4fCombo[2] +=
		  dU4fCombo7 * m_dU2fCombo[2]
		+ dU4fCombo8 * m_dU3fCombo[2];

	m_dU4fCombo[3] += dU4fCombo8 * m_dU3fCombo[3];
	m_dU4fCombo[4] += dU4fCombo8 * m_dU3fCombo[4];
}

///////////////////////////////////////////////////////////////////////////////

void TimestepSchemeARS343b::Step(
	bool fFirstStep,
	bool fLastStep,
	const Time & time,
	double dDeltaT
) {
	// Get a copy of the grid
	Grid * pGrid = m_model.GetGrid();

	// Get a copy of the HorizontalDynamics
	HorizontalDynamics * pHorizontalDynamics = m_model.GetHorizontalDynamics();

	// Get a copy of the VerticalDynamics
	VerticalDynamics * pVerticalDynamics = m_model.GetVerticalDynamics();

	// STAGE 1
	// Compute uf1 into index 1
	pHorizontalDynamics->StepExplicitCombine(
		m_iCopyInst[0], m_dCopyInst[0],
		0, 1, time, m_dDiagExpCf[0] * dDeltaT);
	pGrid->PostProcessSubstage(1, DataType_State);
	pGrid->PostProcessSubstage(1, DataType_Tracers);

	// Compute u1 into index 2
	pHorizontalDynamics->StepImplicitCombine(
		m_iCopyInst[1], m_dCopyInst[1],
		1, 2, time, m_dDiagImpCf[0] * dDeltaT);

	// STAGE 2
	// Compute uf2 from u1 (index 2) into index 3
	pHorizontalDynamics->StepExplicitCombine(
		m_iU2fCombo, m_dU2fCombo,
		2, 3, time, m_dDiagExpCf[1] * dDeltaT);
	pGrid->PostProcessSubstage(3, DataType_State);
	pGrid->PostProcessSubstage(3, DataType_Tracers);

	// Compute u2 from uf2 (index 3) into index 4
	pHorizontalDynamics->StepImplicitCombine(
		m_iCopyInst[3], m_dCopyInst[3],
		3, 4, time, m_dDiagImpCf[1] * dDeltaT);

	// STAGE 3
	// Compute uf3 from u2 (index 4) into index 5
	pHorizontalDynamics->StepExplicitCombine(
		m_iU3fCombo, m_dU3fCombo,
		4, 5, time, m_dDiagExpCf[2] * dDeltaT);
	pGrid->PostProcessSubstage(5, DataType_State);
	pGrid->PostProcessSubstage(5, DataType_Tracers);

	// Compute u3 from uf3 (index 3) into index 6
	pHorizontalDynamics->StepImplicitCombine(
		m_iCopyInst[5], m_dCopyInst[5],
		5, 6, time, m_dDiagImpCf[2] * dDeltaT);

	// STAGE 4
	// Compute uf4 from u3 (index 6) into index 0
	pHorizontalDynamics->StepExplicitCombine(
		m_iU4fCombo, m_dU4fCombo,
		6, 0, time, m_dDiagExpCf[3] * dDeltaT);
	pGrid->PostProcessSubstage(0, DataType_State);
	pGrid->PostProcessSubstage(0, DataType_Tracers);

	// NO IMPLICIT STEP ON THE LAST STAGE

	// Apply hyperdiffusion at the end of the explicit substep
	pHorizontalDynamics->StepAfterSubCycleCombine(
		0, 1, time, dDeltaT);
}

///////////////////////////////////////////////////////////////////////////////

