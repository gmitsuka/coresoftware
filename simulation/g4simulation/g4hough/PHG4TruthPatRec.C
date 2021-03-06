/*!
 *  \file		PHG4TruthPatRec.C
 *  \brief		Truth Pattern Recognition
 *  \details	Using generate, GEANT level information to mimic ideal pattern recognition
 *  \author		Haiwang Yu <yuhw@nmsu.edu>
 */

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/PHTFileServer.h>
#include <g4detectors/PHG4CylinderCellContainer.h>
#include <g4detectors/PHG4CylinderGeomContainer.h>
#include <g4detectors/PHG4CylinderCell_MAPS.h>
#include <g4detectors/PHG4CylinderGeom_MAPS.h>
#include <g4detectors/PHG4CylinderGeom_Siladders.h>
#include <g4main/PHG4Hit.h>
#include <g4main/PHG4HitContainer.h>
#include <g4main/PHG4TruthInfoContainer.h>
#include <g4main/PHG4Particle.h>
#include <g4main/PHG4Particlev2.h>
#include <g4main/PHG4VtxPointv1.h>
#include <GenFit/FieldManager.h>
#include <GenFit/GFRaveVertex.h>
#include <GenFit/GFRaveVertexFactory.h>
#include <GenFit/MeasuredStateOnPlane.h>
#include <GenFit/RKTrackRep.h>
#include <GenFit/StateOnPlane.h>
#include <GenFit/Track.h>
#include <phgenfit/Fitter.h>
#include <phgenfit/PlanarMeasurement.h>
#include <phgenfit/SpacepointMeasurement.h>
#include <phool/getClass.h>
#include <phool/phool.h>
#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNodeIterator.h>
#include <phgeom/PHGeomUtility.h>
#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <map>
#include <memory>

#include "TClonesArray.h"
#include "TMatrixDSym.h"
#include "TTree.h"
#include "TVector3.h"
#include "TRandom3.h"
#include "TRotation.h"

#include "SvtxCluster.h"
#include "SvtxClusterMap.h"
#include "SvtxTrackState_v1.h"
#include "SvtxHit_v1.h"
#include "SvtxHitMap.h"
#include "SvtxTrack.h"
#include "SvtxTrack_FastSim.h"
#include "SvtxVertex_v1.h"
#include "SvtxTrackMap.h"
#include "SvtxTrackMap_v1.h"
#include "SvtxVertexMap_v1.h"
#include "PHG4TruthPatRec.h"

#define LogDebug(exp)		std::cout<<"DEBUG: "  <<__FILE__<<": "<<__LINE__<<": "<< exp <<"\n"
#define LogError(exp)		std::cout<<"ERROR: "  <<__FILE__<<": "<<__LINE__<<": "<< exp <<"\n"
#define LogWarning(exp)	std::cout<<"WARNING: "<<__FILE__<<": "<<__LINE__<<": "<< exp <<"\n"

#define WILD_FLOAT -9999.

using namespace std;

PHG4TruthPatRec::PHG4TruthPatRec(const std::string& name) :
	SubsysReco(name),
//	_detector_type(MAPS_IT_TPC),
//	_use_ladder_maps(false),
//	_use_ladder_intt(false),
	_min_clusters_per_track(10),
	_truth_container(NULL),
	_clustermap(NULL),
	_trackmap(NULL)
{
	_event = 0;
}

PHG4TruthPatRec::~PHG4TruthPatRec() {
}

int PHG4TruthPatRec::Init(PHCompositeNode* topNode) {
	return Fun4AllReturnCodes::EVENT_OK;
}

int PHG4TruthPatRec::InitRun(PHCompositeNode* topNode) {

	CreateNodes(topNode);

	return Fun4AllReturnCodes::EVENT_OK;
}

int PHG4TruthPatRec::process_event(PHCompositeNode* topNode) {

	GetNodes(topNode);

	PHG4HitContainer* phg4hits_svtx = findNode::getClass<PHG4HitContainer>(
			topNode, "G4HIT_SVTX");

	PHG4HitContainer* phg4hits_maps = findNode::getClass<PHG4HitContainer>(
			topNode, "G4HIT_MAPS");

	PHG4HitContainer* phg4hits_intt = findNode::getClass<PHG4HitContainer>(
			topNode, "G4HIT_SILICON_TRACKER");

	if (!phg4hits_svtx and !phg4hits_maps and !phg4hits_intt) {
		if (verbosity >= 0) {
			LogError("!phg4hits_svtx and !phg4hits_maps and !phg4hits_intt");
		}
		return Fun4AllReturnCodes::ABORTRUN;
	}

	SvtxHitMap* hitsmap = NULL;
	// get node containing the digitized hits
	hitsmap = findNode::getClass<SvtxHitMap>(topNode, "SvtxHitMap");
	if (!hitsmap) {
		cout << PHWHERE << "ERROR: Can't find node SvtxHitMap" << endl;
		return Fun4AllReturnCodes::ABORTRUN;
	}

	PHG4CylinderCellContainer* cells_svtx = findNode::getClass<
			PHG4CylinderCellContainer>(topNode, "G4CELL_SVTX");

	PHG4CylinderCellContainer* cells_maps = findNode::getClass<
			PHG4CylinderCellContainer>(topNode, "G4CELL_MAPS");

	PHG4CylinderCellContainer* cells_intt = findNode::getClass<
			PHG4CylinderCellContainer>(topNode, "G4CELL_SILICON_TRACKER");

	if (!cells_svtx and !cells_maps and !cells_intt) {
		if (verbosity >= 0) {
			LogError("!cells_svtx and !cells_maps and !cells_intt");
		}
		return Fun4AllReturnCodes::ABORTRUN;
	}

	typedef std::map< int, std::set<SvtxCluster*> > TrkClustersMap;
	TrkClustersMap m_trackID_clusters;

	for (SvtxClusterMap::ConstIter cluster_itr = _clustermap->begin();
			cluster_itr != _clustermap->end(); cluster_itr++) {
		SvtxCluster *cluster = cluster_itr->second;
		SvtxHit* svtxhit = hitsmap->find(*cluster->begin_hits())->second;
		PHG4CylinderCell* cell = NULL;

		if(!cell and cells_svtx) cell = cells_svtx->findCylinderCell(svtxhit->get_cellid());
		if(!cell and cells_intt) cell = cells_intt->findCylinderCell(svtxhit->get_cellid());
		if(!cell and cells_maps) cell = cells_maps->findCylinderCell(svtxhit->get_cellid());

		if(!cell){
			if(verbosity >= 1) {
				LogError("!cell");
			}
			continue;
		}

		//cell->identify();

		for(PHG4CylinderCell::EdepConstIterator hits_it = cell->get_g4hits().first;
				hits_it != cell->get_g4hits().second; hits_it++){

			PHG4Hit *phg4hit = NULL;
			if(!phg4hit and phg4hits_svtx) phg4hit = phg4hits_svtx->findHit(hits_it->first);
			if(!phg4hit and phg4hits_intt) phg4hit = phg4hits_intt->findHit(hits_it->first);
			if(!phg4hit and phg4hits_maps) phg4hit = phg4hits_maps->findHit(hits_it->first);

			if(!phg4hit){
				if(verbosity >= 1) {
					LogError("!phg4hit");
				}
				continue;
			}

			//phg4hit->identify();

			int particle_id = phg4hit->get_trkid();

			TrkClustersMap::iterator it = m_trackID_clusters.find(particle_id);

			if(it != m_trackID_clusters.end()){
				it->second.insert(cluster);
			} else {
				std::set<SvtxCluster*> clusters;
				clusters.insert(cluster);
				m_trackID_clusters.insert(std::pair< int, std::set<SvtxCluster*> >(particle_id,clusters));
			}
		}
	}

	for (TrkClustersMap::const_iterator trk_clusers_itr = m_trackID_clusters.begin();
			trk_clusers_itr!=m_trackID_clusters.end(); trk_clusers_itr++) {
		if(trk_clusers_itr->second.size() > _min_clusters_per_track) {
			std::unique_ptr<SvtxTrack_FastSim> svtx_track(new SvtxTrack_FastSim());
			//SvtxTrack_FastSim* svtx_track = new SvtxTrack_FastSim();
			svtx_track->set_truth_track_id(trk_clusers_itr->first);
			//to make through minimum pT cut
			svtx_track->set_px(10.);
			svtx_track->set_py(0.);
			svtx_track->set_pz(0.);
			for(SvtxCluster *cluster : trk_clusers_itr->second) {
				svtx_track->insert_cluster(cluster->get_id());
			}
			_trackmap->insert(svtx_track.get());
		}
	}

	if (verbosity >= 2) {
		for (SvtxTrackMap::Iter iter = _trackmap->begin();
				iter != _trackmap->end(); ++iter) {
			SvtxTrack* svtx_track = iter->second;
			svtx_track->identify();
			continue;
			//Print associated clusters;
			for (SvtxTrack::ConstClusterIter iter =
					svtx_track->begin_clusters();
					iter != svtx_track->end_clusters(); ++iter) {
				unsigned int cluster_id = *iter;
				SvtxCluster* cluster = _clustermap->get(cluster_id);
				float radius = sqrt(
						cluster->get_x() * cluster->get_x()
								+ cluster->get_y() * cluster->get_y());
				cout << "Track ID: " << svtx_track->get_id() << ", Track pT: "
						<< svtx_track->get_pt() << ", Particle ID: "
						<< svtx_track->get_truth_track_id() << ", cluster ID: "
						<< cluster->get_id() << ", cluster radius: " << radius
						<< endl;
			}
		}
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

int PHG4TruthPatRec::End(PHCompositeNode* topNode) {
	return Fun4AllReturnCodes::EVENT_OK;
}

int PHG4TruthPatRec::GetNodes(PHCompositeNode* topNode) {
	//DST objects
	//Truth container
	_truth_container = findNode::getClass<PHG4TruthInfoContainer>(topNode,
			"G4TruthInfo");
	if (!_truth_container && _event < 2) {
		cout << PHWHERE << " PHG4TruthInfoContainer node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Input Svtx Clusters
	_clustermap = findNode::getClass<SvtxClusterMap>(topNode, "SvtxClusterMap");
	if (!_clustermap && _event < 2) {
		cout << PHWHERE << " SvtxClusterMap node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Input Svtx Tracks
	_trackmap = findNode::getClass<SvtxTrackMap>(topNode, "SvtxTrackMap");
	if (!_trackmap && _event < 2) {
		cout << PHWHERE << " SvtxTrackMap node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

int PHG4TruthPatRec::CreateNodes(PHCompositeNode* topNode) {

	// create nodes...
	PHNodeIterator iter(topNode);

	PHCompositeNode *dstNode = static_cast<PHCompositeNode*>(iter.findFirst(
			"PHCompositeNode", "DST"));
	if (!dstNode) {
		cerr << PHWHERE << "DST Node missing, doing nothing." << endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}
	PHNodeIterator iter_dst(dstNode);

	// Create the SVTX node
	PHCompositeNode* tb_node =
			dynamic_cast<PHCompositeNode*>(iter_dst.findFirst("PHCompositeNode",
					"SVTX"));
	if (!tb_node) {
		tb_node = new PHCompositeNode("SVTX");
		dstNode->addNode(tb_node);
		if (verbosity > 0)
			cout << "SVTX node added" << endl;
	}

	_trackmap = new SvtxTrackMap_v1;
	PHIODataNode<PHObject>* tracks_node = new PHIODataNode<PHObject>(_trackmap,
			"SvtxTrackMap", "PHObject");
	tb_node->addNode(tracks_node);
	if (verbosity > 0)
		cout << "Svtx/SvtxTrackMap node added" << endl;

	return Fun4AllReturnCodes::EVENT_OK;
}
