///////////////////////////////////////////////////////////////////////////////
///
///	\file    ExtractSurface.cpp
///	\author  Paul Ullrich
///	\version July 8, 2014
///
///	<remarks>
///		Copyright 2000-2010 Paul Ullrich
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#include "DataVector.h"
#include "DataMatrix.h"
#include "DataMatrix3D.h"
#include "CommandLine.h"
#include "Announce.h"

#include <cstdlib>
#include <netcdfcpp.h>

#include "mpi.h"

////////////////////////////////////////////////////////////////////////////////

void CopyNcFileAttributes(
	NcFile * fileIn,
	NcFile * fileOut
) {
	for (int a = 0; a < fileIn->num_atts(); a++) {
		NcAtt * att = fileIn->get_att(a);
		long num_vals = att->num_vals();

		NcValues * pValues = att->values();

		if (att->type() == ncByte) {
			fileOut->add_att(att->name(), num_vals,
				(const ncbyte*)(pValues->base()));

		} else if (att->type() == ncChar) {
			fileOut->add_att(att->name(), num_vals,
				(const char*)(pValues->base()));

		} else if (att->type() == ncShort) {
			fileOut->add_att(att->name(), num_vals,
				(const short*)(pValues->base()));

		} else if (att->type() == ncInt) {
			fileOut->add_att(att->name(), num_vals,
				(const int*)(pValues->base()));

		} else if (att->type() == ncFloat) {
			fileOut->add_att(att->name(), num_vals,
				(const float*)(pValues->base()));

		} else if (att->type() == ncDouble) {
			fileOut->add_att(att->name(), num_vals,
				(const double*)(pValues->base()));

		} else {
			_EXCEPTIONT("Invalid attribute type");
		}

		delete pValues;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CopyNcVarAttributes(
	NcVar * varIn,
	NcVar * varOut
) {
	for (int a = 0; a < varIn->num_atts(); a++) {
		NcAtt * att = varIn->get_att(a);
		long num_vals = att->num_vals();

		NcValues * pValues = att->values();

		if (att->type() == ncByte) {
			varOut->add_att(att->name(), num_vals,
				(const ncbyte*)(pValues->base()));

		} else if (att->type() == ncChar) {
			varOut->add_att(att->name(), num_vals,
				(const char*)(pValues->base()));

		} else if (att->type() == ncShort) {
			varOut->add_att(att->name(), num_vals,
				(const short*)(pValues->base()));

		} else if (att->type() == ncInt) {
			varOut->add_att(att->name(), num_vals,
				(const int*)(pValues->base()));

		} else if (att->type() == ncFloat) {
			varOut->add_att(att->name(), num_vals,
				(const float*)(pValues->base()));

		} else if (att->type() == ncDouble) {
			varOut->add_att(att->name(), num_vals,
				(const double*)(pValues->base()));

		} else {
			_EXCEPTIONT("Invalid attribute type");
		}

		delete pValues;
	}
}

///////////////////////////////////////////////////////////////////////////////

void ParsePressureLevels(
	const std::string & strPressureLevels,
	std::vector<double> & vecPressureLevels
) {
	int iPressureBegin = 0;
	int iPressureCurrent = 0;

	// Parse pressure levels
	bool fRangeMode = false;
	for (;;) {
		if ((iPressureCurrent >= strPressureLevels.length()) ||
			(strPressureLevels[iPressureCurrent] == ',') ||
			(strPressureLevels[iPressureCurrent] == ' ') ||
			(strPressureLevels[iPressureCurrent] == ':')
		) {
			// Range mode
			if ((!fRangeMode) &&
				(strPressureLevels[iPressureCurrent] == ':')
			) {
				if (vecPressureLevels.size() != 0) {
					_EXCEPTIONT("Invalid set of pressure levels");
				}
				fRangeMode = true;
			}
			if (fRangeMode) {
				if ((strPressureLevels[iPressureCurrent] != ':') &&
					(iPressureCurrent < strPressureLevels.length())
				) {
					_EXCEPTION1("Invalid character in pressure range (%c)",
						strPressureLevels[iPressureCurrent]);
				}
			}

			if (iPressureCurrent == iPressureBegin) {
				if (iPressureCurrent >= strPressureLevels.length()) {
					break;
				}

				continue;
			}

			std::string strPressureLevelSubStr = 
				strPressureLevels.substr(
					iPressureBegin, iPressureCurrent - iPressureBegin);

			vecPressureLevels.push_back(atof(strPressureLevelSubStr.c_str()));
			
			iPressureBegin = iPressureCurrent + 1;
		}

		iPressureCurrent++;
	}

	// Range mode -- repopulate array
	if (fRangeMode) {
		if (vecPressureLevels.size() != 3) {
			_EXCEPTIONT("Exactly three pressure level entries required "
				"for range mode");
		}
		double dPressureBegin = vecPressureLevels[0];
		double dPressureStep = vecPressureLevels[1];
		double dPressureEnd = vecPressureLevels[2];

		if (dPressureStep == 0.0) {
			_EXCEPTIONT("Pressure step size cannot be zero");
		}
		if ((dPressureEnd - dPressureBegin) / dPressureStep > 10000.0) {
			_EXCEPTIONT("Too many pressure levels in range (limit 10000)");
		}
		if ((dPressureEnd - dPressureBegin) / dPressureStep < 0.0) {
			_EXCEPTIONT("Sign mismatch in pressure step");
		}

		vecPressureLevels.clear();
		for (int i = 0 ;; i++) {
			double dPressureLevel =
				dPressureBegin + static_cast<double>(i) * dPressureStep;

			if ((dPressureStep > 0.0) && (dPressureLevel > dPressureEnd)) {
				break;
			}
			if ((dPressureStep < 0.0) && (dPressureLevel < dPressureEnd)) {
				break;
			}

			vecPressureLevels.push_back(dPressureLevel);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void ParseVariableList(
	const std::string & strVariables,
	std::vector< std::string > & vecVariableStrings
) {
	int iVarBegin = 0;
	int iVarCurrent = 0;

	// Parse variable name
	for (;;) {
		if ((iVarCurrent >= strVariables.length()) ||
			(strVariables[iVarCurrent] == ',') ||
			(strVariables[iVarCurrent] == ' ')
		) {
			if (iVarCurrent == iVarBegin) {
				if (iVarCurrent >= strVariables.length()) {
					break;
				}

				continue;
			}

			vecVariableStrings.push_back(
				strVariables.substr(iVarBegin, iVarCurrent - iVarBegin));

			iVarBegin = iVarCurrent + 1;
		}

		iVarCurrent++;
	}
}

///////////////////////////////////////////////////////////////////////////////

void InterpolationWeightsLinear(
	double dP,
	const DataVector<double> & dataP,
	int & kBegin,
	int & kEnd,
	DataVector<double> & dW
) {
	const int nLev = dataP.GetRows();

	if (dP > dataP[0]) {
		kBegin = 0;
		kEnd = 2;

		dW[0] = (dataP[1] - dP)
		      / (dataP[1] - dataP[0]);

		dW[1] = 1.0 - dW[k];

	} else if (dP < dataP[nLev-1]) {
		kBegin = nLev-1;
		kEnd = nLev;
		dW[nLev-1] = 1.0;

	} else {
		for (int k = 0; k < nLev-1; k++) {
			if (dP >= dataP[k+1]) {
				kBegin = k;
				kEnd = k+2;

				dW[k] = (dataP[k+1] - dP)
				      / (dataP[k+1] - dataP[k]);

				dW[k+1] = 1.0 - dW[k];

				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char ** argv) {

	// Initialize MPI
	MPI_Init(&argc, &argv);

try {

	// Input filename
	std::string strInputFile;

	// Output filename
	std::string strOutputFile;

	// Separate topography file
	std::string strTopographyFile;

	// List of variables to extract
	std::string strVariables;

	// Extract geopotential height
	bool fGeopotentialHeight;

	// Pressure levels to extract
	std::string strPressureLevels;

	// Extract variables at the surface
	bool fExtractSurface;

	// Extract total energy
	bool fExtractTotalEnergy;

	// Parse the command line
	BeginCommandLine()
		CommandLineString(strInputFile, "in", "");
		CommandLineString(strOutputFile, "out", "");
		CommandLineString(strVariables, "var", "");
		CommandLineBool(fGeopotentialHeight, "output_z");
		CommandLineBool(fExtractTotalEnergy, "output_energy");
		CommandLineString(strPressureLevels, "p", "0.0");
		CommandLineBool(fExtractSurface, "surf");

		ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	AnnounceBanner();

	// Check command line arguments
	if (strInputFile == "") {
		_EXCEPTIONT("No input file specified");
	}
	if (strOutputFile == "") {
		_EXCEPTIONT("No output file specified");
	}
	if (strVariables == "") {
		_EXCEPTIONT("No variables specified");
	}

	// Parse variable string
	std::vector< std::string > vecVariableStrings;

	ParseVariableList(strVariables, vecVariableStrings);

	// Check variables
	if (vecVariableStrings.size() == 0) {
		_EXCEPTIONT("No variables specified");
	}

	// Parse pressure level string
	std::vector<double> vecPressureLevels;

	ParsePressureLevels(strPressureLevels, vecPressureLevels);

	int nPressureLevels = (int)(vecPressureLevels.size());

	// Check pressure levels
	if (nPressureLevels == 0) {
		_EXCEPTIONT("No pressure levels to process");
	}

	// Open input file
	AnnounceStartBlock("Loading input file");
	NcFile ncdf_in(strInputFile.c_str(), NcFile::ReadOnly);
	if (!ncdf_in.is_valid()) {
		_EXCEPTION1("Unable to open file \"%s\" for reading",
			strInputFile.c_str());
	}

	// Load time array
	Announce("Time");
	NcVar * varTime = ncdf_in.get_var("time");
	int nTime = varTime->get_dim(0)->size();

	DataVector<double> dTime;
	dTime.Initialize(nTime);
	varTime->set_cur((long)0);
	varTime->get(&(dTime[0]), nTime);

	// Load latitude array
	Announce("Latitude");
	NcVar * varLat = ncdf_in.get_var("lat");
	int nLat = varLat->get_dim(0)->size();

	DataVector<double> dLat;
	dLat.Initialize(nLat);
	varLat->set_cur((long)0);
	varLat->get(&(dLat[0]), nLat);

	// Load longitude array
	Announce("Longitude");
	NcVar * varLon = ncdf_in.get_var("lon");
	int nLon = varLon->get_dim(0)->size();

	DataVector<double> dLon;
	dLon.Initialize(nLon);
	varLon->set_cur((long)0);
	varLon->get(&(dLon[0]), nLon);

	// Load level array
	Announce("Level");
	NcVar * varLev = ncdf_in.get_var("lev");
	int nLev = varLev->get_dim(0)->size();

	DataVector<double> dLev;
	dLev.Initialize(nLev);
	varLev->set_cur((long)0);
	varLev->get(&(dLev[0]), nLev);

	// Load topography
	Announce("Topography");
	NcVar * varZs = ncdf_in.get_var("Zs");

	DataMatrix<double> dZs;
	dZs.Initialize(nLat, nLon);
	varZs->set_cur((long)0, (long)0);
	varZs->get(&(dZs[0][0]), nLat, nLon);

	AnnounceEndBlock("Done");

	// Open output file
	AnnounceStartBlock("Constructing output file");

	NcFile ncdf_out(strOutputFile.c_str(), NcFile::Replace);
	if (!ncdf_out.is_valid()) {
		_EXCEPTION1("Unable to open file \"%s\" for writing",
			strOutputFile.c_str());
	}

	CopyNcFileAttributes(&ncdf_in, &ncdf_out);

	// Output time array
	Announce("Time");
	NcDim * dimOutTime = ncdf_out.add_dim("time");
	NcVar * varOutTime = ncdf_out.add_var("time", ncDouble, dimOutTime);
	varOutTime->set_cur((long)0);
	varOutTime->put(&(dTime[0]), nTime);

	CopyNcVarAttributes(varTime, varOutTime);

	// Output pressure array
	Announce("Pressure");
	NcDim * dimOutP = ncdf_out.add_dim("p", nPressureLevels);
	NcVar * varOutP = ncdf_out.add_var("p", ncDouble, dimOutP);
	varOutP->set_cur((long)0);
	varOutP->put(&(vecPressureLevels[0]), nPressureLevels);

	// Output latitude and longitude array
	Announce("Latitude");
	NcDim * dimOutLat = ncdf_out.add_dim("lat", nLat);
	NcVar * varOutLat = ncdf_out.add_var("lat", ncDouble, dimOutLat);
	varOutLat->set_cur((long)0);
	varOutLat->put(&(dLat[0]), nLat);

	CopyNcVarAttributes(varLat, varOutLat);

	Announce("Longitude");
	NcDim * dimOutLon = ncdf_out.add_dim("lon", nLon);
	NcVar * varOutLon = ncdf_out.add_var("lon", ncDouble, dimOutLon);
	varOutLon->set_cur((long)0);
	varOutLon->put(&(dLon[0]), nLon);

	CopyNcVarAttributes(varLon, varOutLon);

	// Done
	AnnounceEndBlock("Done");

	// Load all variables
	Announce("Loading variables");

	std::vector<NcVar *> vecNcVar;
	for (int v = 0; v < vecVariableStrings.size(); v++) {
		vecNcVar.push_back(ncdf_in.get_var(vecVariableStrings[v].c_str()));
		if (vecNcVar[v] == NULL) {
			_EXCEPTION1("Unable to load variable \"%s\" from file",
				vecVariableStrings[v].c_str());
		}
	}

	// Physical constants
	Announce("Initializing thermodynamic variables");

	NcAtt * attEarthRadius = ncdf_in.get_att("earth_radius");
	double dEarthRadius = attEarthRadius->as_double(0);

	NcAtt * attRd = ncdf_in.get_att("Rd");
	double dRd = attRd->as_double(0);

	NcAtt * attCp = ncdf_in.get_att("Cp");
	double dCp = attCp->as_double(0);

	double dGamma = dCp / (dCp - dRd);

	NcAtt * attP0 = ncdf_in.get_att("P0");
	double dP0 = attP0->as_double(0);

	double dPressureScaling = dP0 * pow(dRd / dP0, dGamma);

	NcAtt * attZtop = ncdf_in.get_att("Ztop");
	double dZtop = attZtop->as_double(0);

	// Input data
	DataMatrix3D<double> dataIn;
	dataIn.Initialize(nLev, nLat, nLon);

	// Output data
	DataMatrix<double> dataOut;
	dataOut.Initialize(nLat, nLon);

	// Pressure in column
	DataVector<double> dataColumnP;
	dataColumnP.Initialize(nLev);

	// Column weights
	DataVector<double> dW;
	dW.Initialize(nLev);

	// Loop through all times, pressure levels and variables
	AnnounceStartBlock("Interpolating");

	// Create new variables
	std::vector<NcVar *> vecOutNcVar;
	for (int v = 0; v < vecNcVar.size(); v++) {
		vecOutNcVar.push_back(
			ncdf_out.add_var(
				vecVariableStrings[v].c_str(), ncDouble,
					dimOutTime, dimOutP, dimOutLat, dimOutLon));

		// Copy attributes
		CopyNcVarAttributes(vecNcVar[v], vecOutNcVar[v]);
	}

	// Add energy variable
	NcVar * varEnergy;
	if (fExtractTotalEnergy) {
		varEnergy = ncdf_out.add_var("TE", ncDouble, dimOutTime);
	}

	// Loop over all times
	for (int t = 0; t < nTime; t++) {

		char szAnnounce[256];
		sprintf(szAnnounce, "Time %i", t); 
		AnnounceStartBlock(szAnnounce);

		// Rho
		DataMatrix3D<double> dataRho;
		dataRho.Initialize(nLev, nLat, nLon);

		NcVar * varRho = ncdf_in.get_var("Rho");
		varRho->set_cur(t, 0, 0, 0);
		varRho->get(&(dataRho[0][0][0]), 1, nLev, nLat, nLon);

		// Theta
		DataMatrix3D<double> dataTheta;
		dataTheta.Initialize(nLev, nLat, nLon);

		NcVar * varTheta = ncdf_in.get_var("Theta");
		varTheta->set_cur(t, 0, 0, 0);
		varTheta->get(&(dataTheta[0][0][0]), 1, nLev, nLat, nLon);

		// Pressure everywhere
		DataMatrix3D<double> dataP;
		dataP.Initialize(nLev, nLat, nLon);

		for (int k = 0; k < nLev; k++) {
		for (int i = 0; i < nLat; i++) {
		for (int j = 0; j < nLon; j++) {
			dataP[k][i][j] = dPressureScaling
				* exp(log(dataRho[k][i][j] * dataTheta[k][i][j]) * dGamma);
		}
		}
		}

		// Loop through all pressure levels and variables
		for (int v = 0; v < vecOutNcVar.size(); v++) {

			Announce(vecVariableStrings[v].c_str());

			for (int p = 0; p < nPressureLevels; p++) {

				// Load in the data array
				vecNcVar[v]->set_cur(t, 0, 0, 0);
				vecNcVar[v]->get(&(dataIn[0][0][0]), 1, nLev, nLat, nLon);

				// Loop thorugh all latlon indices
				for (int i = 0; i < nLat; i++) {
				for (int j = 0; j < nLon; j++) {

					// Find weights
					int kBegin = 0;
					int kEnd = 0;

					// On a pressure surface
					if (vecPressureLevels[p] != 0.0) {
						for (int k = 0; k < nLev; k++) {
							dataColumnP[k] = dataP[k][i][j];
						}

						InterpolationWeightsLinear(
							vecPressureLevels[p],
							dataColumnP,
							kBegin,
							kEnd,
							dW);
					}

					// At the physical surface
					if (fExtractSurface) {
						kBegin = 0;
						kEnd = 2;

						dW[0] =   dLev[1] / (dLev[1] - dLev[0]);
						dW[1] = - dLev[0] / (dLev[1] - dLev[0]);
/*
						kBegin = 0;
						kEnd = 4;

						dW[0] =  1.726913829;
						dW[1] = -1.072997307;
						dW[2] =  0.4105364237;
						dW[3] = -0.06445294633;
*/
					}

/*
					// DEBUG
					double dTestRho = 0.0;
					double dTestTheta = 0.0;
					double dP;
					for (int k = kBegin; k < kEnd; k++) {
						dTestRho += dW[k] * dataRho[k][i][j];
						dTestTheta += dW[k] * dataTheta[k][i][j];
					}

					dP = dPressureScaling
						* exp(log(dTestRho * dTestTheta) * dGamma);

					printf("%1.5e %1.5e %1.5e\n", dataP[kBegin], dP, dataP[kEnd-1]);
*/
					// Interpolate in the vertical
					dataOut[i][j] = 0.0;
					for (int k = kBegin; k < kEnd; k++) {
						dataOut[i][j] += dW[k] * dataIn[k][i][j];
					}
				}
				}

				// Write variable
				vecOutNcVar[v]->set_cur(t, p, 0, 0);
				vecOutNcVar[v]->put(&(dataOut[0][0]), 1, 1, nLat, nLon);

			}
		}

		// Extract total energy
		if (fExtractTotalEnergy) {
			Announce("Total Energy");

			// Zonal velocity
			DataMatrix3D<double> dataU;
			dataU.Initialize(nLev, nLat, nLon);

			NcVar * varU = ncdf_in.get_var("U");
			varU->set_cur(t, 0, 0, 0);
			varU->get(&(dataU[0][0][0]), 1, nLev, nLat, nLon);

			// Meridional velocity
			DataMatrix3D<double> dataV;
			dataV.Initialize(nLev, nLat, nLon);

			NcVar * varV = ncdf_in.get_var("V");
			varV->set_cur(t, 0, 0, 0);
			varV->get(&(dataV[0][0][0]), 1, nLev, nLat, nLon);

			// Vertical velocity
			DataMatrix3D<double> dataW;
			dataW.Initialize(nLev, nLat, nLon);

			NcVar * varW = ncdf_in.get_var("W");
			varW->set_cur(t, 0, 0, 0);
			varW->get(&(dataW[0][0][0]), 1, nLev, nLat, nLon);

			// Calculate total energy
			double dTotalEnergy = 0.0;

			double dElementRefArea =
				dEarthRadius * dEarthRadius
				* M_PI / static_cast<double>(nLat)
				* 2.0 * M_PI / static_cast<double>(nLon);

			for (int k = 0; k < nLev; k++) {
			for (int i = 0; i < nLat; i++) {
			for (int j = 0; j < nLon; j++) {
				double dKineticEnergy =
					0.5 * dataRho[k][i][j] *
						( dataU[k][i][j] * dataU[k][i][j]
						+ dataV[k][i][j] * dataV[k][i][j]
						+ dataW[k][i][j] * dataW[k][i][j]);

				double dInternalEnergy =
					dataP[k][i][j] / (dGamma - 1.0);

				dTotalEnergy +=
					(dKineticEnergy + dInternalEnergy)
						* cos(M_PI * dLat[i] / 180.0) * dElementRefArea
						* (dZtop - dZs[i][j]) / static_cast<double>(nLev);
			}
			}
			}

			// Put total energy into file
			varEnergy->set_cur(t);
			varEnergy->put(&dTotalEnergy, 1);
		}

		AnnounceEndBlock("Done");
	}
/*
	// Output geopotential height
	if (fGeopotentialHeight) {

		Announce("Geopotential height");

		// Loop thorugh all latlon indices
		for (int i = 0; i < nLat; i++) {
		for (int j = 0; j < nLon; j++) {

			int kBegin;
			int kEnd;

			// Interpolate onto pressure level
			if (dPressureLevel != 0.0) {
				for (int k = 0; k < nLev; k++) {
					dataColumnP[k] = dataP[k][i][j];
				}

				InterpolationWeightsLinear(
					dPressureLevel,
					dataColumnP,
					kBegin,
					kEnd,
					dW);
			}

			// At the physical surface
			if (fExtractSurface) {
				kBegin = 0;
				kEnd = 2;

				dW[0] =   dLev[1] / (dLev[1] - dLev[0]);
				dW[1] = - dLev[0] / (dLev[1] - dLev[0]);
			}

			dataOut[i][j] = 0.0;
			for (int k = kBegin; k < kEnd; k++) {
				dataOut[i][j] += dW[k] * dLev[k];
			}

			dataOut[i][j] = dZs[i][j] + dataOut[i][j] * (dZtop - dZs[i][j]);
		}
		}

		// Write variable
		NcVar * varOut = ncdf_out.add_var(
			"Z", ncDouble, dimOutLat, dimOutLon);

		varOut->set_cur(0, 0);
		varOut->put(&(dataOut[0][0]), nLat, nLon);
	}
*/

	AnnounceEndBlock("Done");

} catch(Exception & e) {
	Announce(e.ToString().c_str());
}

	// Finalize MPI
	MPI_Finalize();

}

///////////////////////////////////////////////////////////////////////////////

