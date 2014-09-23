#include <iostream>
#include <map>

#include "TROOT.h"
#include "TApplication.h"

#include "TFile.h"
#include "TChain.h"
#include "TString.h"

#include "TH2.h"
#include "THStack.h"
#include "TLegend.h"
#include "TPaveText.h"
#include "TLorentzVector.h"
#include "TVector3.h"
#include "TF1.h"
#include "TF2.h"
#include "TRandom3.h"

#include "LHEF.h"

#include "ExRootClasses.h"
#include "ExRootTreeWriter.h"
#include "ExRootTreeBranch.h"

#include "ExRootUtilities.h"
#include "ExRootProgressBar.h"

#include "KinUtils.h"

using namespace std;

KinUtils *m_utils;
//---------------------------------------------------------------------------

double AnalyseParticles(LHEF::Reader *reader) {
	LHEF::HEPEUP &hepeup = reader->hepeup;
	const LHEF::HEPRUP &heprup = reader->heprup;
	long PID;
	Int_t particle, n_inside;

	Double_t signPz, cosTheta,M;
	Double_t w,L,ndet,sigma;
	Double_t xthetaxfmin, xthetaxfmax, xthetayfmin, xthetayfmax;
	TLorentzVector chi, recoil_chi, recoil_elastic;
	TVector3 vin, vhit, fiducialV;
	vector<double> tmp_v;

	n_inside = 0;
	xthetaxfmin = xthetayfmin = 0;
	xthetaxfmax = (heprup.lx / (2 * heprup.ldet)); //ok without ATAN for this check
	xthetayfmax = (heprup.ly / (2 * heprup.ldet)); //ok without ATAN for this check

	//set the fiducial volume "box", with respect to the z axis.
	fiducialV.SetXYZ(heprup.lx/2, heprup.ly/2, heprup.lz);

	w=hepeup.XWGTUP; //this is the event weight, in pbarn, as given by Madgraph.
	ndet=heprup.NDET;
	for (particle = 0; particle < hepeup.NUP; ++particle) {

		PID = hepeup.IDUP[particle];
		if ((PID != -611) && (PID != 611)){
			continue; //We are not interested in other particles. Go on
		}
		M = hepeup.PUP[particle][4];
		chi.SetPxPyPzE(hepeup.PUP[particle][0], hepeup.PUP[particle][1],
				hepeup.PUP[particle][2], hepeup.PUP[particle][3]);
		cosTheta = TMath::Abs(chi.CosTheta());
		signPz = (chi.Pz() >= 0.0) ? 1.0 : -1.0;
		/* I need now to apply the fiducial cuts.
		 I need to find which of the chis are inside (if more than one), and use that for the interaction.
		 If both are inside, I take only one.*/
		if ((fabs(chi.Px() / chi.Pz()) < xthetaxfmax)
				&& (fabs(chi.Py() / chi.Pz()) < xthetayfmax))
			n_inside++;
		else { //for now, just mark this chi out of the fiducial volume with a status "0"
			hepeup.ISTUP[particle] = 0;
			continue;
		}
		//now check if this is the second chi, if it is within the fiducial volume, and if the first was already in the fiducial volume. If so, we skip the second chi
		if (n_inside == 2) {
			hepeup.ISTUP[particle] = 0;
			continue;
		}
		vin.SetXYZ((chi.Px() / chi.Pz()) * heprup.ldet,
				(chi.Py() / chi.Pz()) * heprup.ldet, heprup.ldet); //the chi hit position in the fiducial volume front-face
		//From here, we have a chi within the fiducial volume.
		//See which interaction to consider

		switch (heprup.procid) {
		case Proc_nothing: //nothing to do
			w=0;
			break;
		case Proc_Pelastic: //proton elastic
		case Proc_Eelastic: //electron elastic
			sigma=m_utils->doElasticRecoil(chi, recoil_elastic, recoil_chi,heprup.procid);
			L=m_utils->findInteractionPoint(chi, fiducialV, vin, vhit);
			w=w*L*heprup.NDET*sigma;
			//add particles to hepeup
			//final state chi
			hepeup.IDUP.push_back(9611);
			hepeup.ISTUP.push_back(1);
			hepeup.MOTHUP.push_back(std::make_pair(particle + 1, particle + 1));
			hepeup.ICOLUP.push_back(std::make_pair(0, 0));
			tmp_v.clear();
			tmp_v.push_back(recoil_chi.Px());
			tmp_v.push_back(recoil_chi.Py());
			tmp_v.push_back(recoil_chi.Pz());
			tmp_v.push_back(recoil_chi.E());
			tmp_v.push_back(recoil_chi.M());
			hepeup.PUP.push_back(tmp_v);
			hepeup.VTIMUP.push_back(0);
			hepeup.SPINUP.push_back(0);
			//final state recoil
			(heprup.procid == Proc_Pelastic ? hepeup.IDUP.push_back(9212) : hepeup.IDUP.push_back(911));
			hepeup.ISTUP.push_back(1);
			hepeup.MOTHUP.push_back(std::make_pair(particle + 1, particle + 1));
			hepeup.ICOLUP.push_back(std::make_pair(0, 0));
			tmp_v.clear();
			tmp_v.push_back(recoil_elastic.Px());
			tmp_v.push_back(recoil_elastic.Py());
			tmp_v.push_back(recoil_elastic.Pz());
			tmp_v.push_back(recoil_elastic.E());
			tmp_v.push_back(recoil_elastic.M());
			hepeup.PUP.push_back(tmp_v);
			hepeup.VTIMUP.push_back(0);
			hepeup.SPINUP.push_back(0);
			hepeup.NUP += 2;
			break;
		case Proc_Pinelastic: //inelastic
		case Proc_Einelastic:
			break;
		default:
			cout << "Error, interaction not recognized" << endl;
			break;
		}

		//use the eventComments for the vertex location (in m, in the form x y z)
		reader->eventComments = Form("%f %f %f", vhit.X(), vhit.Y(), vhit.Z());
	}
	return w*n_inside*1E-36; //by multipling per n_inside, automatically I correct for the fact I have two chis, potentially both in the detector.
	//the factor 1E-36 is the conversion pbarn ---> cm2
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
	char appName[] = "ExRootLHEFConverter";

	if (argc != 3) {
		cout << " Usage: " << appName << " input_file" << " output_file"
				<< endl;
		cout << " input_file - input file in LHEF format," << endl;
		cout << " output_file - output file in LHEF format." << endl;
		return 1;
	}

	gROOT->SetBatch();

	int appargc = 1;
	char *appargv[] = { appName };
	double W,L;
	TApplication app(appName, &appargc, appargv);

	// Open a stream connected to an event file:
	ifstream inputFileStream(argv[1]);
	ofstream outputFileStream(argv[2]);

	// Create the Reader object:
	LHEF::Reader *inputReader = new LHEF::Reader(inputFileStream); //this triggers also "init"
	// Create the Writer object:
	LHEF::Writer *outputWriter = new LHEF::Writer(outputFileStream);
	outputWriter->heprup = inputReader->heprup;
	outputWriter->headerStream.str(inputReader->headerBlock);
	outputWriter->init();

	//instear of having, for each process, a single function with a free parameter (the incident chi energy),
	//it is much better to have 100 of them, each for a certain energy: otherwise, the integral gets calculated for each event!
	cout << " ** Preparing the cross-section functions and the KinUtils class ** " << endl;

	//note that alphaEM is saved in the event header (altough we are not going to touch this at all!).
	//Therefore, I will set it in the first event in the  loop.
	m_utils=new KinUtils(inputReader->heprup.EBEAM,inputReader->heprup.FMASS,inputReader->heprup.APMASS,0,inputReader->heprup.EPSILON,inputReader->heprup.SEED);


	cout << "** Calculating number of events to process. Please wait..."
			<< endl;
	Long64_t allEntries = inputReader->getNumberOfEvents();
	cout << "** Input file contains " << allEntries << " events" << endl;
	W=0;
	if (allEntries > 0) {
		ExRootProgressBar progressBar(allEntries);

		// Loop over all events
		Long64_t entry = 0;
		while (inputReader->readEvent()) {

			if (entry==0){
				m_utils->setAlpha(inputReader->hepeup.AQEDUP); //set alpha_EM. It is saved evnt by evnt (but we have it fixed!)
				gRandom->SetSeed(inputReader->heprup.SEED);
			}
			//This is the function that triggers the interaction in the fiducial volume.
			if (inputReader->heprup.procid)
				W+=AnalyseParticles(inputReader); //this also returns the "corrected" event weight (production weight * interaction probability)
				outputWriter->hepeup = inputReader->hepeup;
				outputWriter->eventStream.str(inputReader->eventComments);
				outputWriter->writeEvent();
				progressBar.Update(entry);
				++entry;
		}

		progressBar.Finish();
	}
	W=W*(inputReader->heprup.NDUMP*inputReader->heprup.LDUMP);
	cout<< " Events per EOT: "<<W<<endl;
	cout << "** Exiting..." << endl;

	delete inputReader;
	delete outputWriter;
}

