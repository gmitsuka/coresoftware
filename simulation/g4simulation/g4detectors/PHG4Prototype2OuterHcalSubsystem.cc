#include "PHG4Prototype2OuterHcalSubsystem.h"
#include "PHG4Prototype2OuterHcalDetector.h"
#include "PHG4EventActionClearZeroEdep.h"
#include "PHG4Prototype2OuterHcalSteppingAction.h"
#include "PHG4Parameters.h"

#include <g4main/PHG4HitContainer.h>

#include <phool/getClass.h>

#include <Geant4/globals.hh>

#include <boost/foreach.hpp>

#include <set>
#include <sstream>

using namespace std;

//_______________________________________________________________________
PHG4Prototype2OuterHcalSubsystem::PHG4Prototype2OuterHcalSubsystem( const std::string &name, const int lyr ):
  PHG4DetectorSubsystem( name, lyr ),
  detector_(NULL),
  steppingAction_( NULL ),
  eventAction_(NULL)
{
  InitializeParameters();
}

//_______________________________________________________________________
int 
PHG4Prototype2OuterHcalSubsystem::InitRunSubsystem( PHCompositeNode* topNode )
{
  PHNodeIterator iter( topNode );
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST" ));

  // create detector
  detector_ = new PHG4Prototype2OuterHcalDetector(topNode, GetParams(), Name());
  detector_->SuperDetector(SuperDetector());
  detector_->OverlapCheck(CheckOverlap());
  set<string> nodes;
  if (GetParams()->get_int_param("active"))
    {
      PHNodeIterator dstiter(dstNode);
      PHCompositeNode *DetNode = dynamic_cast<PHCompositeNode*>(dstiter.findFirst("PHCompositeNode",SuperDetector()));
      if (! DetNode)
	{
          DetNode = new PHCompositeNode(SuperDetector());
          dstNode->addNode(DetNode);
        }

      ostringstream nodename;
      if (SuperDetector() != "NONE")
	{
	  nodename <<  "G4HIT_" << SuperDetector();
	}
      else
	{
	  nodename <<  "G4HIT_" << Name();
	}
      nodes.insert(nodename.str());
      if (GetParams()->get_int_param("absorberactive"))
	{
	  nodename.str("");
	  if (SuperDetector() != "NONE")
	    {
	      nodename <<  "G4HIT_ABSORBER_" << SuperDetector();
	    }
	  else
	    {
	      nodename <<  "G4HIT_ABSORBER_" << Name();
	    }
          nodes.insert(nodename.str());
	}
      BOOST_FOREACH(string node, nodes)
	{
	  PHG4HitContainer* g4_hits =  findNode::getClass<PHG4HitContainer>( topNode , node.c_str());
	  if ( !g4_hits )
	    {
	      g4_hits = new PHG4HitContainer(node);
              DetNode->addNode( new PHIODataNode<PHObject>( g4_hits, node.c_str(), "PHObject" ));
	    }
	  if (! eventAction_)
	    {
	      eventAction_ = new PHG4EventActionClearZeroEdep(topNode, node);
	    }
	  else
	    {
	      PHG4EventActionClearZeroEdep *evtact = dynamic_cast<PHG4EventActionClearZeroEdep *>(eventAction_);

	      evtact->AddNode(node);
	    }
	}

      // create stepping action
      steppingAction_ = new PHG4Prototype2OuterHcalSteppingAction(detector_, GetParams());

    }
  else
    {
      // if this is a black hole it does not have to be active
      if (GetParams()->get_int_param("blackhole"))
	{
	  steppingAction_ = new PHG4Prototype2OuterHcalSteppingAction(detector_, GetParams());
	}
    }
  return 0;

}

//_______________________________________________________________________
int
PHG4Prototype2OuterHcalSubsystem::process_event( PHCompositeNode * topNode )
{
  // pass top node to stepping action so that it gets
  // relevant nodes needed internally
  if (steppingAction_)
    {
      steppingAction_->SetInterfacePointers( topNode );
    }
  return 0;
}


void
PHG4Prototype2OuterHcalSubsystem::Print(const string &what) const
{
  cout << Name() << " Parameters: " << endl;
  GetParams()->Print();
  if (detector_)
    {
      detector_->Print(what);
    }
  return;
}

//_______________________________________________________________________
PHG4Detector* PHG4Prototype2OuterHcalSubsystem::GetDetector( void ) const
{
  return detector_;
}

void
PHG4Prototype2OuterHcalSubsystem::SetDefaultParameters()
{
  // all in cm
  set_default_double_param("light_balance_inner_corr", NAN);
  set_default_double_param("light_balance_inner_radius", NAN);
  set_default_double_param("light_balance_outer_corr", NAN);
  set_default_double_param("light_balance_outer_radius", NAN);
  set_default_double_param("place_x", 0.);
  set_default_double_param("place_y", 0.);
  set_default_double_param("place_z", 0.);
  set_default_double_param("rot_x", 0.);
  set_default_double_param("rot_y", 0.);
  set_default_double_param("rot_z", 0.);
  set_default_double_param("steplimits", NAN);

  set_default_int_param("light_scint_model", 1);
  set_default_int_param("hi_eta", 0);

  set_default_string_param("material", "SS310");

}

void
PHG4Prototype2OuterHcalSubsystem::SetLightCorrection(const double inner_radius, const double inner_corr,const double outer_radius, const double outer_corr)
{
  set_double_param("light_balance_inner_corr", inner_corr);
  set_double_param("light_balance_inner_radius", inner_radius);
  set_double_param("light_balance_outer_corr", outer_corr);
  set_double_param("light_balance_outer_radius", outer_radius);
  return;
}
