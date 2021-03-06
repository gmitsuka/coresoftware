#include "PHG4CylinderCellTPCReco.h"
#include "PHG4CylinderGeomContainer.h"
#include "PHG4CylinderGeom.h"
#include "PHG4CylinderCellGeomContainer.h"
#include "PHG4CylinderCellGeom.h"
#include "PHG4CylinderCellv1.h"
#include "PHG4CylinderCellContainer.h"
#include "PHG4CylinderCellDefs.h"
#include "PHG4TPCDistortion.h"

#include <g4main/PHG4Hit.h>
#include <g4main/PHG4HitContainer.h>
#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/Fun4AllServer.h>

#include <phool/getClass.h>
#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHRandomSeed.h>

#include <CLHEP/Units/PhysicalConstants.h>
#include <CLHEP/Units/SystemOfUnits.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <limits>

using namespace std;

PHG4CylinderCellTPCReco::PHG4CylinderCellTPCReco(int n_pixel,
                                                 const string &name)
    : SubsysReco(name),
      _timer(PHTimeServer::get()->insert_new(name)),
      diffusion(0.0057),
      elec_per_kev(38.),
      driftv(6.0/1000.0), // cm per ns
      num_pixel_layers(n_pixel),
      tmin_default(0.0),  // ns
      tmax_default(60.0), // ns
      tmin_max(),
      distortion(NULL) 
{
  memset(nbins,0,sizeof(nbins));
  rand.SetSeed(PHRandomSeed());
}

PHG4CylinderCellTPCReco::~PHG4CylinderCellTPCReco()
{
  delete distortion;
}

void PHG4CylinderCellTPCReco::Detector(const std::string &d)
{
  detector = d;
  // only set the outdetector nodename if it wasn't set already
  // in case the order in the macro is outdetector(); detector();
  if (outdetector.size() == 0)
  {
    OutputDetector(d);
  }
  return;
}


void PHG4CylinderCellTPCReco::cellsize(const int i, const double sr, const double sz)
{
  cell_size[i] = std::make_pair(sr, sz);
}


int PHG4CylinderCellTPCReco::InitRun(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  
  PHCompositeNode *dstNode;
  dstNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode){std::cout << PHWHERE << "DST Node missing, doing nothing." << std::endl;exit(1);}
  hitnodename = "G4HIT_" + detector;
  PHG4HitContainer *g4hit = findNode::getClass<PHG4HitContainer>(topNode, hitnodename.c_str());
  if (!g4hit){cout << "Could not locate g4 hit node " << hitnodename << endl;exit(1);}
  cellnodename = "G4CELL_" + outdetector;
  PHG4CylinderCellContainer *cells = findNode::getClass<PHG4CylinderCellContainer>(topNode , cellnodename);
  if (!cells){cells = new PHG4CylinderCellContainer();PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(cells, cellnodename.c_str() , "PHObject");dstNode->addNode(newNode);}
  geonodename = "CYLINDERGEOM_" + detector;
  PHG4CylinderGeomContainer *geo =  findNode::getClass<PHG4CylinderGeomContainer>(topNode , geonodename.c_str());
  if (!geo){cout << "Could not locate geometry node " << geonodename << endl;exit(1);}
  
  seggeonodename = "CYLINDERCELLGEOM_" + outdetector;
  PHG4CylinderCellGeomContainer *seggeo = findNode::getClass<PHG4CylinderCellGeomContainer>(topNode , seggeonodename.c_str());
  if (!seggeo){seggeo = new PHG4CylinderCellGeomContainer();PHCompositeNode *runNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "RUN" ));PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(seggeo, seggeonodename.c_str() , "PHObject");runNode->addNode(newNode);}
  
  map<int, PHG4CylinderGeom *>::const_iterator miter;
  pair <map<int, PHG4CylinderGeom *>::const_iterator, map<int, PHG4CylinderGeom *>::const_iterator> begin_end = geo->get_begin_end();
  map<int, std::pair <double, double> >::iterator sizeiter;
  for(miter = begin_end.first; miter != begin_end.second; ++miter)
  {
    PHG4CylinderGeom *layergeom = miter->second;
    int layer = layergeom->get_layer();
    double circumference = layergeom->get_radius() * 2 * M_PI;
    double length_in_z = layergeom->get_zmax() - layergeom->get_zmin();
    sizeiter = cell_size.find(layer);
    if (sizeiter == cell_size.end()){cout << "no cell sizes for layer " << layer << endl;exit(1);}
    PHG4CylinderCellGeom *layerseggeo = new PHG4CylinderCellGeom();
    layerseggeo->set_layer(layergeom->get_layer());
    layerseggeo->set_radius(layergeom->get_radius());
    layerseggeo->set_thickness(layergeom->get_thickness());
    
    zmin_max[layer] = make_pair(layergeom->get_zmin(), layergeom->get_zmax());
    double size_z = (sizeiter->second).second;
    double size_r = (sizeiter->second).first;
    double bins_r;
    // unlikely but if the circumference is a multiple of the cell size
    // use result of division, if not - add 1 bin which makes the
    // cells a tiny bit smaller but makes them fit
    double fract = modf(circumference / size_r, &bins_r);
    if (fract != 0)
    {
      bins_r++;
    }
    nbins[0] = bins_r;
    size_r = circumference / bins_r;
    (sizeiter->second).first = size_r;
    double phistepsize = 2 * M_PI / bins_r;
    double phimin = -M_PI;
    double phimax = phimin + phistepsize;
    phistep[layer] = phistepsize;
    for (int i = 0 ; i < nbins[0]; i++)
    {
      phimax += phistepsize;
    }
    // unlikely but if the length is a multiple of the cell size
    // use result of division, if not - add 1 bin which makes the
    // cells a tiny bit smaller but makes them fit
    fract = modf(length_in_z / size_z, &bins_r);
    if (fract != 0)
    {
      bins_r++;
    }
    nbins[1] = bins_r;
    pair<int, int> phi_z_bin = make_pair(nbins[0], nbins[1]);
    n_phi_z_bins[layer] = phi_z_bin;
    // update our map with the new sizes
    size_z = length_in_z / bins_r;
    (sizeiter->second).second = size_z;
    double zlow = layergeom->get_zmin();
    double zhigh = zlow + size_z;;
    for (int i = 0 ; i < nbins[1]; i++)
    {
      zhigh += size_z;
    }
    layerseggeo->set_binning(PHG4CylinderCellDefs::sizebinning);
    layerseggeo->set_zbins(nbins[1]);
    layerseggeo->set_zmin(layergeom->get_zmin());
    layerseggeo->set_zstep(size_z);
    layerseggeo->set_phibins(nbins[0]);
    layerseggeo->set_phistep(phistepsize);
    
    seggeo->AddLayerCellGeom(layerseggeo);
  }

  for (std::map<int,int>::iterator iter = binning.begin(); 
       iter != binning.end(); ++iter) {
    int layer = iter->first;
    // if the user doesn't set an integration window, set the default
    tmin_max.insert(std::make_pair(layer,std::make_pair(tmin_default,tmax_default)));    
  }
  
  return Fun4AllReturnCodes::EVENT_OK;
}



int PHG4CylinderCellTPCReco::process_event(PHCompositeNode *topNode)
{
  _timer.get()->restart();
  PHG4HitContainer *g4hit = findNode::getClass<PHG4HitContainer>(topNode, hitnodename.c_str());
  if (!g4hit){cout << "Could not locate g4 hit node " << hitnodename << endl;exit(1);}
  PHG4CylinderCellContainer *cells = findNode::getClass<PHG4CylinderCellContainer>(topNode, cellnodename);
  if (! cells){cout << "could not locate cell node " << cellnodename << endl;exit(1);}
  PHG4CylinderCellGeomContainer *seggeo = findNode::getClass<PHG4CylinderCellGeomContainer>(topNode , seggeonodename.c_str());
  if (! seggeo){cout << "could not locate geo node " << seggeonodename << endl;exit(1);}
  
  map<int, std::pair <double, double> >::iterator sizeiter;
  PHG4HitContainer::LayerIter layer;
  pair<PHG4HitContainer::LayerIter, PHG4HitContainer::LayerIter> layer_begin_end = g4hit->getLayers();
  
  for(layer = layer_begin_end.first; layer != layer_begin_end.second; layer++)
  {
    std::map<unsigned long long, PHG4CylinderCell*> cellptmap;
    PHG4HitContainer::ConstIterator hiter;
    PHG4HitContainer::ConstRange hit_begin_end = g4hit->getHits(*layer);
    PHG4CylinderCellGeom *geo = seggeo->GetLayerCellGeom(*layer);
    int nphibins = n_phi_z_bins[*layer].first;
    int nzbins = n_phi_z_bins[*layer].second;

    sizeiter = cell_size.find(*layer);
    if (sizeiter == cell_size.end()){cout << "logical screwup!!! no sizes for layer " << *layer << endl;exit(1);}
    double zstepsize = (sizeiter->second).second;
    double phistepsize = phistep[*layer];
    for (hiter = hit_begin_end.first; hiter != hit_begin_end.second; hiter++)
    {
      // checking ADC timing integration window cut
      if (hiter->second->get_t(0)>tmin_max[*layer].second) continue;
      if (hiter->second->get_t(1)<tmin_max[*layer].first) continue;
      
      double xinout;
      double yinout;
      double phi;
      double z;
      int phibin;
      int zbin;
      xinout = hiter->second->get_avg_x();
      yinout = hiter->second->get_avg_y();
      double r = sqrt( xinout*xinout + yinout*yinout );
      phi = atan2(hiter->second->get_avg_y(), hiter->second->get_avg_x());
      z =  hiter->second->get_avg_z();
      
      // apply primary charge distortion
      if( (*layer) >= (unsigned int)num_pixel_layers )
        { // in TPC	    
          if (distortion)
            {
              // do TPC distortion

              const double dz = distortion ->get_z_distortion(r,phi,z);
              const double drphi = distortion ->get_rphi_distortion(r,phi,z);
              //TODO: radial distortion is not applied at the moment,
              //      because it leads to major change to the structure of this code and it affect the insensitive direction to
              //      near radial tracks
              //
              //          const double dr = distortion ->get_r_distortion(r,phi,z);
              phi += drphi/r;
              z += dz;
            }

      //TODO: this is an approximation of average track propagation time correction on a cluster's hit time or z-position.
      // Full simulation require implement this correction in PHG4TPCClusterizer::process_event
      const double approximate_cluster_path_length = sqrt(
		hiter->second->get_avg_x() * hiter->second->get_avg_x()
		+ hiter->second->get_avg_y() * hiter->second->get_avg_y()
		+ hiter->second->get_avg_z() * hiter->second->get_avg_z());
	  const double speed_of_light_cm_ns = CLHEP::c_light / (CLHEP::centimeter / CLHEP::nanosecond);
	  if (z >= 0.0)
		z -= driftv * ( hiter->second->get_avg_t() - approximate_cluster_path_length / speed_of_light_cm_ns);
	  else
		z += driftv * ( hiter->second->get_avg_t() - approximate_cluster_path_length / speed_of_light_cm_ns);
      }

      phibin = geo->get_phibin( phi );
      if(phibin < 0 || phibin >= nphibins){continue;}
      double phidisp = phi - geo->get_phicenter(phibin);
      
      zbin = geo->get_zbin( hiter->second->get_avg_z() );
      if(zbin < 0 || zbin >= nzbins){continue;}
      double zdisp = z - geo->get_zcenter(zbin);
      
      double edep = hiter->second->get_edep();
      
      if( (*layer) < (unsigned int)num_pixel_layers )
	{
	  unsigned long long tmp = phibin;
	  unsigned long long inkey = tmp << 32;
	  inkey += zbin;
	  PHG4CylinderCell *cell = nullptr;
	  map<unsigned long long, PHG4CylinderCell*>::iterator it;
	  it = cellptmap.find(inkey);
	  if (it != cellptmap.end())
	    {
	      cell = it->second;
	    }
	  else
	    {
	      cell = new PHG4CylinderCellv1();
	      cellptmap[inkey] = cell;
	      cell->set_layer(*layer);
	      cell->set_phibin(phibin);
	      cell->set_zbin(zbin);
	    }
          cell->add_edep(hiter->first, edep);
          cell->add_shower_edep(hiter->second->get_shower_id(), edep);
        }
      else
      {
        double nelec = elec_per_kev*1.0e6*edep;

        double cloud_sig_x = 1.5*sqrt( diffusion*diffusion*(100. - fabs(hiter->second->get_avg_z())) + 0.03*0.03 );
        double cloud_sig_z = 1.5*sqrt((1.+2.2*2.2)*diffusion*diffusion*(100. - fabs(hiter->second->get_avg_z())) + 0.01*0.01 );
        
        int n_phi = (int)(3.*( cloud_sig_x/(r*phistepsize) )) + 3;
        int n_z = (int)(3.*( cloud_sig_z/zstepsize )) + 3;
        
        double cloud_sig_x_inv = 1./cloud_sig_x;
        double cloud_sig_z_inv = 1./cloud_sig_z;
        
        // we will store effective number of electrons instead of edep
        for( int iphi = -n_phi; iphi <= n_phi; ++iphi )
        {
          int cur_phi_bin = phibin + iphi;
          if( cur_phi_bin < 0 ){cur_phi_bin += nphibins;}
          else if( cur_phi_bin >= nphibins ){cur_phi_bin -= nphibins;}
          
          if( (cur_phi_bin < 0) || (cur_phi_bin >= nphibins) ){continue;}
        
          
          double phi_integral = 0.5*erf(-0.5*sqrt(2.)*phidisp*r*cloud_sig_x_inv + 0.5*sqrt(2.)*( (0.5 + (double)iphi)*phistepsize*r )*cloud_sig_x_inv) - 0.5*erf(-0.5*sqrt(2.)*phidisp*r*cloud_sig_x_inv + 0.5*sqrt(2.)*( (-0.5 + (double)iphi)*phistepsize*r )*cloud_sig_x_inv);
          
          for( int iz = -n_z; iz <= n_z; ++iz )
          {
            int cur_z_bin = zbin + iz;if( (cur_z_bin < 0) || (cur_z_bin >= nzbins) ){continue;}
            
            double z_integral = 0.5*erf(-0.5*sqrt(2.)*zdisp*cloud_sig_z_inv + 0.5*sqrt(2.)*( (0.5 + (double)iz)*zstepsize )*cloud_sig_z_inv) - 0.5*erf(-0.5*sqrt(2.)*zdisp*cloud_sig_z_inv + 0.5*sqrt(2.)*( (-0.5 + (double)iz)*zstepsize )*cloud_sig_z_inv);

            double total_weight = rand.Poisson( nelec*( phi_integral * z_integral ) );
            
            if( !(total_weight == total_weight) ){continue;}
            if(total_weight == 0.){continue;}
	    unsigned long long tmp = cur_phi_bin;
	    unsigned long long inkey = tmp << 32;
	    inkey += cur_z_bin;
	    PHG4CylinderCell *cell = nullptr;
	    map<unsigned long long, PHG4CylinderCell*>::iterator it;
	    it = cellptmap.find(inkey);
	    if (it != cellptmap.end())
	      {
		cell = it->second;
	      }
	    else
	      {
		cell = new PHG4CylinderCellv1();
		cellptmap[inkey] = cell;
		cell->set_layer(*layer);
		cell->set_phibin(cur_phi_bin);
		cell->set_zbin(cur_z_bin);
	      }
	    cell->add_edep(hiter->first, total_weight);
	    cell->add_shower_edep(hiter->second->get_shower_id(), total_weight);
	  }
	}
      }
    }
    int count = 0;
    for(std::map<unsigned long long, PHG4CylinderCell*>::iterator it = cellptmap.begin(); it != cellptmap.end(); ++it)
    {
      cells->AddCylinderCell((unsigned int)(*layer), it->second);
      count += 1;
    }
  }
  // cout<<"PHG4CylinderCellTPCReco end"<<endl;
  _timer.get()->stop();
  return Fun4AllReturnCodes::EVENT_OK;
}


int PHG4CylinderCellTPCReco::End(PHCompositeNode *topNode)
{
  return Fun4AllReturnCodes::EVENT_OK;
}


