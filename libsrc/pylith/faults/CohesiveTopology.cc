// -*- C++ -*-
//
// ----------------------------------------------------------------------
//
// Brad T. Aagaard, U.S. Geological Survey
// Charles A. Williams, GNS Science
// Matthew G. Knepley, University of Chicago
//
// This code was developed as part of the Computational Infrastructure
// for Geodynamics (http://geodynamics.org).
//
// Copyright (c) 2010-2014 University of California, Davis
//
// See COPYING for license information.
//
// ----------------------------------------------------------------------
//

#include <portinfo>

#include "CohesiveTopology.hh" // implementation of object methods

#include "TopologyOps.hh" // USES TopologyOps
#include "pylith/topology/Mesh.hh" // USES Mesh

#include <cassert> // USES assert()

#include "pylith/utils/error.h" // USES PYLITH_CHECK_ERROR

// ----------------------------------------------------------------------
void
pylith::faults::CohesiveTopology::createFault(topology::Mesh* faultMesh,
                                              PetscDM& faultBoundary,
                                              const topology::Mesh& mesh,
                                              PetscDMLabel groupField)
{ // createFault
  PYLITH_METHOD_BEGIN;

  assert(faultMesh);
  PetscErrorCode err;

  faultMesh->coordsys(mesh.coordsys());
  PetscDM dmMesh = mesh.dmMesh();assert(dmMesh);

  PetscInt dim, depth, gdepth;
  err = DMPlexGetDimension(dmMesh, &dim);PYLITH_CHECK_ERROR(err);
  err = DMPlexGetDepth(dmMesh, &depth);PYLITH_CHECK_ERROR(err);

  // Convert fault to a DM
  err = MPI_Allreduce(&depth, &gdepth, 1, MPIU_INT, MPI_MAX, mesh.comm());PYLITH_CHECK_ERROR(err);
  if (gdepth == dim) {
    PetscDM subdm = NULL;
    PetscDMLabel label = NULL;
    const char *groupName = "", *labelName = "boundary";

    if (groupField) {err = DMLabelGetName(groupField, &groupName);PYLITH_CHECK_ERROR(err);}
    err = DMPlexCreateSubmesh(dmMesh, groupField, 1, &subdm);PYLITH_CHECK_ERROR(err);
    err = DMPlexOrient(subdm);PYLITH_CHECK_ERROR(err);

    err = DMPlexCreateLabel(subdm, labelName);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetLabel(subdm, labelName, &label);PYLITH_CHECK_ERROR(err);
    err = DMPlexMarkBoundaryFaces(subdm, label);PYLITH_CHECK_ERROR(err);
    err = DMPlexCreateSubmesh(subdm, label, 1, &faultBoundary);PYLITH_CHECK_ERROR(err);
    std::string submeshLabel = "fault_" + std::string(groupName);
    faultMesh->dmMesh(subdm, submeshLabel.c_str());
  } else {
    PetscDM faultDMMeshTmp = NULL, faultDMMesh = NULL;
    PetscDMLabel subpointMapTmp = NULL, subpointMap = NULL;
    PetscIS pointIS = NULL;
    const PetscInt *points = NULL;
    PetscInt depth, newDepth, h, numPoints = 0, p;
    const char *groupName = "";

    if (groupField) {err = DMLabelGetName(groupField, &groupName);PYLITH_CHECK_ERROR(err);}
    err = DMPlexCreateSubmesh(dmMesh, groupField, 1, &faultDMMeshTmp);PYLITH_CHECK_ERROR(err);
    err = DMPlexInterpolate(faultDMMeshTmp, &faultDMMesh);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetVTKCellHeight(faultDMMeshTmp, &h);PYLITH_CHECK_ERROR(err);
    err = DMPlexSetVTKCellHeight(faultDMMesh, h);PYLITH_CHECK_ERROR(err);
    err = DMPlexOrient(faultDMMesh);PYLITH_CHECK_ERROR(err);
    err = DMPlexCopyCoordinates(faultDMMeshTmp, faultDMMesh);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetSubpointMap(faultDMMeshTmp, &subpointMapTmp);PYLITH_CHECK_ERROR(err);
    err = DMLabelCreate("subpoint_map", &subpointMap);PYLITH_CHECK_ERROR(err);
    err = DMLabelGetStratumIS(subpointMapTmp, 0, &pointIS);PYLITH_CHECK_ERROR(err);
    if (pointIS) {
      err = ISGetLocalSize(pointIS, &numPoints);PYLITH_CHECK_ERROR(err);
      err = ISGetIndices(pointIS, &points);PYLITH_CHECK_ERROR(err);
    }
    for (p = 0; p < numPoints; ++p) {
      err = DMLabelSetValue(subpointMap, points[p], 0);PYLITH_CHECK_ERROR(err);
    }
    if (pointIS) {err = ISRestoreIndices(pointIS, &points);PYLITH_CHECK_ERROR(err);}
    err = ISDestroy(&pointIS);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetDepth(faultDMMeshTmp, &depth);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetDepth(faultDMMesh, &newDepth);PYLITH_CHECK_ERROR(err);
    err = DMLabelGetStratumIS(subpointMapTmp, depth, &pointIS);PYLITH_CHECK_ERROR(err);
    if (pointIS) {
      err = ISGetLocalSize(pointIS, &numPoints);PYLITH_CHECK_ERROR(err);
      err = ISGetIndices(pointIS, &points);PYLITH_CHECK_ERROR(err);
    }
    for (p = 0; p < numPoints; ++p) {
      err = DMLabelSetValue(subpointMap, points[p], newDepth);PYLITH_CHECK_ERROR(err);
    }
    if (pointIS) {err = ISRestoreIndices(pointIS, &points);PYLITH_CHECK_ERROR(err);}
    err = ISDestroy(&pointIS);PYLITH_CHECK_ERROR(err);
    err = DMPlexSetSubpointMap(faultDMMesh, subpointMap);PYLITH_CHECK_ERROR(err);
    err = DMLabelDestroy(&subpointMap);PYLITH_CHECK_ERROR(err);
    err = DMDestroy(&faultDMMeshTmp);PYLITH_CHECK_ERROR(err);
    
    std::string submeshLabel = "fault_" + std::string(groupName);
    faultMesh->dmMesh(faultDMMesh, submeshLabel.c_str());

    PetscDMLabel label = NULL;
    const char *labelName = "boundary";

    err = DMPlexCreateLabel(faultDMMesh, labelName);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetLabel(faultDMMesh, labelName, &label);PYLITH_CHECK_ERROR(err);
    err = DMPlexMarkBoundaryFaces(faultDMMesh, label);PYLITH_CHECK_ERROR(err);
    err = DMPlexCreateSubmesh(faultDMMesh, label, 1, &faultBoundary);PYLITH_CHECK_ERROR(err);
  }

  PYLITH_METHOD_END;
} // createFault

// ----------------------------------------------------------------------
void
pylith::faults::CohesiveTopology::create(topology::Mesh* mesh,
                                         const topology::Mesh& faultMesh,
                                         DM faultBoundary,
                                         DMLabel groupField,
                                         const int materialId,
                                         int& firstFaultVertex,
                                         int& firstLagrangeVertex,
                                         int& firstFaultCell,
                                         const bool constraintCell)
{ // create
  PYLITH_METHOD_BEGIN;

  assert(mesh);

  const char    *groupName = NULL;
  PetscMPIInt    rank;
  PetscErrorCode err;

  err = MPI_Comm_rank(mesh->comm(), &rank);PYLITH_CHECK_ERROR(err);
  if (groupField) {err = DMLabelGetName(groupField, &groupName);PYLITH_CHECK_ERROR(err);}

  /* DMPlex */
  DM complexMesh = mesh->dmMesh();
  DM faultDMMesh = faultMesh.dmMesh();
  assert(complexMesh);assert(faultDMMesh);

  PetscInt depth, cStart = 0, cEnd = 0;
  err = DMPlexGetDepth(complexMesh, &depth);PYLITH_CHECK_ERROR(err);
  err = DMPlexGetHeightStratum(complexMesh, 0, &cStart, &cEnd);PYLITH_CHECK_ERROR(err);
  PetscInt faceSizeDM = 0;
  int numFaultCorners = 0; // The number of vertices in a fault cell
  PetscInt *indicesDM = NULL; // The indices of a face vertex set in a cell
  const int debug = mesh->debug();
  int oppositeVertex = 0;    // For simplices, the vertex opposite a given face
  TopologyOps::PointArray origVertices;
  TopologyOps::PointArray faceVertices;
  TopologyOps::PointArray neighborVertices;
  PetscInt *origVerticesDM = NULL;
  PetscInt *faceVerticesDM = NULL;
  PetscInt cellDim, numCornersDM = 0;

  err = DMPlexGetDimension(complexMesh, &cellDim);PYLITH_CHECK_ERROR(err);
  if (!rank) {
    err = DMPlexGetConeSize(complexMesh, cStart, &numCornersDM);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetNumFaceVertices(complexMesh, cellDim, numCornersDM, &faceSizeDM);PYLITH_CHECK_ERROR(err);
    err = PetscMalloc(faceSizeDM * sizeof(PetscInt), &indicesDM);PYLITH_CHECK_ERROR(err);
    /* TODO: Do we need faceSize at all? Blaise was using a nice criterion */
    PetscInt fStart, numFaultCornersDM;

    err = DMPlexGetHeightStratum(faultDMMesh, 1, &fStart, NULL);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetConeSize(faultDMMesh, fStart, &numFaultCornersDM);PYLITH_CHECK_ERROR(err);
    numFaultCorners = numFaultCornersDM;
  }

  // Add new shadow vertices and possibly Lagrange multipler vertices
  IS              fVertexIS   = NULL;
  const PetscInt *fVerticesDM = NULL;
  PetscInt        numFaultVerticesDM = 0, vStart, vEnd;

  if (groupField) {
    err = DMLabelGetStratumIS(groupField, 1, &fVertexIS);PYLITH_CHECK_ERROR(err);
    err = ISGetLocalSize(fVertexIS, &numFaultVerticesDM);PYLITH_CHECK_ERROR(err);
    err = ISGetIndices(fVertexIS, &fVerticesDM);PYLITH_CHECK_ERROR(err);
  }
  err = DMPlexGetDepthStratum(complexMesh, 0, &vStart, &vEnd);PYLITH_CHECK_ERROR(err);
  std::map<point_type,point_type> vertexRenumber;
  std::map<point_type,point_type> vertexLagrangeRenumber;
  std::map<point_type,point_type> cellRenumber;
  std::map<point_type,point_type> vertexRenumberDM;
  std::map<point_type,point_type> vertexLagrangeRenumberDM;
  std::map<point_type,point_type> cellRenumberDM;
  PetscInt ffStart, ffEnd, numFaultFacesDM, fvtStart, fvtEnd;
  PetscInt faultVertexOffsetDM, firstFaultVertexDM, firstLagrangeVertexDM, firstFaultCellDM, extraVertices, extraCells;

  err = DMPlexGetDepthStratum(faultDMMesh, 0, &fvtStart, &fvtEnd);PYLITH_CHECK_ERROR(err);
  err = DMPlexGetHeightStratum(faultDMMesh, 1, &ffStart, &ffEnd);PYLITH_CHECK_ERROR(err);
  numFaultFacesDM = ffEnd - ffStart;
  /* TODO This will have to change for multiple faults */
  PetscInt numNormalCells, numCohesiveCells, numNormalVertices, numShadowVertices, numLagrangeVertices;

  extraVertices         = numFaultVerticesDM * (constraintCell ? 2 : 1); /* Total number of fault vertices on this fault (shadow + Lagrange) */
  extraCells            = numFaultFacesDM;                               /* Total number of fault cells */
  firstFaultVertexDM    = vEnd + extraCells;
  firstLagrangeVertexDM = firstFaultVertexDM + numFaultVerticesDM * (constraintCell ? 1 : 0);
  firstFaultCellDM      = cEnd;
  mesh->getPointTypeSizes(&numNormalCells, &numCohesiveCells, &numNormalVertices, &numShadowVertices, &numLagrangeVertices);
  faultVertexOffsetDM   = numCohesiveCells;
  if (!numNormalCells) {
    mesh->setPointTypeSizes(cEnd-cStart, extraCells, vEnd-vStart, firstLagrangeVertex, constraintCell ? firstLagrangeVertex : 0);
  } else {
    mesh->setPointTypeSizes(numNormalCells, numCohesiveCells+extraCells, numNormalVertices, numShadowVertices+firstLagrangeVertex, constraintCell ? numLagrangeVertices+firstLagrangeVertex : 0);
  }
  if (firstFaultVertex == 0) {
    PetscInt pStart, pEnd;

    err = DMPlexGetChart(complexMesh, &pStart, &pEnd);PYLITH_CHECK_ERROR(err);
    firstFaultVertex     = pEnd - pStart;
    firstLagrangeVertex += firstFaultVertex;
    firstFaultCell      += firstFaultVertex;
  }

  /* DMPlex */
  DM        newMesh;
  PetscInt *newCone = NULL;
  PetscInt  dim, maxConeSize = 0;

  err = DMCreate(mesh->comm(), &newMesh);PYLITH_CHECK_ERROR(err);
  err = DMSetType(newMesh, DMPLEX);PYLITH_CHECK_ERROR(err);
  err = DMPlexGetDimension(complexMesh, &dim);PYLITH_CHECK_ERROR(err);
  err = DMPlexSetDimension(newMesh, dim);PYLITH_CHECK_ERROR(err);
  err = DMPlexSetChart(newMesh, 0, firstFaultVertexDM + extraVertices);PYLITH_CHECK_ERROR(err);
  for(PetscInt c = cStart; c < cEnd; ++c) {
    PetscInt coneSize;
    err = DMPlexGetConeSize(complexMesh, c, &coneSize);PYLITH_CHECK_ERROR(err);
    err = DMPlexSetConeSize(newMesh, c, coneSize);PYLITH_CHECK_ERROR(err);
    maxConeSize = PetscMax(maxConeSize, coneSize);
  }
  for(PetscInt c = cEnd; c < cEnd+numFaultFacesDM; ++c) {
    err = DMPlexSetConeSize(newMesh, c, constraintCell ? faceSizeDM*3 : faceSizeDM*2);PYLITH_CHECK_ERROR(err);
  }
  err = DMSetUp(newMesh);PYLITH_CHECK_ERROR(err);
  err = PetscMalloc(maxConeSize * sizeof(PetscInt), &newCone);PYLITH_CHECK_ERROR(err);
  for(PetscInt c = cStart; c < cEnd; ++c) {
    const PetscInt *cone = NULL;
    PetscInt        coneSize, cp;

    err = DMPlexGetCone(complexMesh, c, &cone);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetConeSize(complexMesh, c, &coneSize);PYLITH_CHECK_ERROR(err);
    for(cp = 0; cp < coneSize; ++cp) {
      newCone[cp] = cone[cp] + extraCells;
    }
    err = DMPlexSetCone(newMesh, c, newCone);PYLITH_CHECK_ERROR(err);
  }
  err = PetscFree(newCone);PYLITH_CHECK_ERROR(err);
  PetscInt cMax, vMax;

  err = DMPlexGetHybridBounds(complexMesh, &cMax, NULL, NULL, &vMax);PYLITH_CHECK_ERROR(err);
  if (cMax < 0) {
    err = DMPlexSetHybridBounds(newMesh, firstFaultCellDM, PETSC_DETERMINE, PETSC_DETERMINE, PETSC_DETERMINE);PYLITH_CHECK_ERROR(err);
  } else {
    err = DMPlexSetHybridBounds(newMesh, cMax, PETSC_DETERMINE, PETSC_DETERMINE, PETSC_DETERMINE);PYLITH_CHECK_ERROR(err);
  }
  if (vMax < 0) {
    err = DMPlexSetHybridBounds(newMesh, PETSC_DETERMINE, PETSC_DETERMINE, PETSC_DETERMINE, firstLagrangeVertexDM);PYLITH_CHECK_ERROR(err);
  } else {
    err = DMPlexSetHybridBounds(newMesh, PETSC_DETERMINE, PETSC_DETERMINE, PETSC_DETERMINE, vMax+extraCells);PYLITH_CHECK_ERROR(err);
  }

  // TODO: Use DMPlexGetLabels(): Renumber labels
  PetscInt    numLabels;
  std::string skip = "depth";

  err = DMPlexGetNumLabels(complexMesh, &numLabels);PYLITH_CHECK_ERROR(err);
  for (PetscInt l = 0; l < numLabels; ++l) {
    const char     *lname = NULL;
    IS              idIS = NULL;
    PetscInt        n;
    const PetscInt *ids = NULL;

    err = DMPlexGetLabelName(complexMesh, l, &lname);PYLITH_CHECK_ERROR(err);
    if (std::string(lname) == skip) continue;
    err = DMPlexGetLabelSize(complexMesh, lname, &n);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetLabelIdIS(complexMesh, lname, &idIS);PYLITH_CHECK_ERROR(err);
    err = ISGetIndices(idIS, &ids);PYLITH_CHECK_ERROR(err);
    for(PetscInt i = 0; i < n; ++i) {
      const PetscInt  id = ids[i];
      const PetscInt *points = NULL;
      IS              sIS = NULL;
      PetscInt        size;

      err = DMPlexGetStratumSize(complexMesh, lname, id, &size);PYLITH_CHECK_ERROR(err);
      err = DMPlexGetStratumIS(complexMesh, lname, id, &sIS);PYLITH_CHECK_ERROR(err);
      err = ISGetIndices(sIS, &points);PYLITH_CHECK_ERROR(err);
      for(PetscInt s = 0; s < size; ++s) {
        if ((points[s] >= vStart) && (points[s] < vEnd)) {
          err = DMPlexSetLabelValue(newMesh, lname, points[s]+extraCells, id);PYLITH_CHECK_ERROR(err);
        } else {
          err = DMPlexSetLabelValue(newMesh, lname, points[s], id);PYLITH_CHECK_ERROR(err);
        }
      }
      err = ISRestoreIndices(sIS, &points);PYLITH_CHECK_ERROR(err);
      err = ISDestroy(&sIS);PYLITH_CHECK_ERROR(err);
    }
    err = ISRestoreIndices(idIS, &ids);PYLITH_CHECK_ERROR(err);
    err = ISDestroy(&idIS);PYLITH_CHECK_ERROR(err);
  } // for

  // Add fault vertices to groups and construct renumberings
  std::string skipA = "depth";
  std::string skipB = "material-id";

  err = DMPlexGetNumLabels(complexMesh, &numLabels);PYLITH_CHECK_ERROR(err);
  for (PetscInt fv = 0; fv < numFaultVerticesDM; ++fv, ++firstFaultVertexDM) {
    const PetscInt v    = fVerticesDM[fv];
    const PetscInt vnew = v+extraCells;

    vertexRenumberDM[vnew] = firstFaultVertexDM;
    err = DMPlexSetLabelValue(newMesh, groupName, firstFaultVertexDM, 1);PYLITH_CHECK_ERROR(err);
    if (constraintCell) {
      vertexLagrangeRenumberDM[vnew] = firstLagrangeVertexDM;
      err = DMPlexSetLabelValue(newMesh, groupName, firstLagrangeVertexDM, 1);PYLITH_CHECK_ERROR(err);
      ++firstLagrangeVertexDM;
    } // if

    // Add shadow vertices to other groups, don't add constraint
    // vertices (if they exist) because we don't want BC, etc to act
    // on constraint vertices
    for (PetscInt l = 0; l < numLabels; ++l) {
      const char *name = NULL;
      PetscInt    value;

      err = DMPlexGetLabelName(complexMesh, l, &name);PYLITH_CHECK_ERROR(err);
      if (std::string(name) == skipA) continue;
      if (std::string(name) == skipB) continue;

      err = DMPlexGetLabelValue(newMesh, name, vnew, &value);PYLITH_CHECK_ERROR(err);
      if (value != -1) {
        err = DMPlexSetLabelValue(newMesh, name, vertexRenumberDM[vnew], value);PYLITH_CHECK_ERROR(err);
      }
    }
  } // for

  // Split the mesh along the fault mesh and create cohesive elements
  const PetscInt firstCohesiveCellDM = firstFaultCellDM;
  TopologyOps::PointSet replaceCells;
  TopologyOps::PointSet noReplaceCells;
  TopologyOps::PointSet replaceVerticesDM;
  PetscInt       *cohesiveCone = NULL;
  IS              subpointIS = NULL;
  const PetscInt *subpointMap = NULL;

  err = DMPlexCreateSubpointIS(faultDMMesh, &subpointIS);PYLITH_CHECK_ERROR(err);
  if (subpointIS) {err = ISGetIndices(subpointIS, &subpointMap);PYLITH_CHECK_ERROR(err);}
  err = PetscMalloc3(faceSizeDM,&origVerticesDM,faceSizeDM,&faceVerticesDM,faceSizeDM*3,&cohesiveCone);PYLITH_CHECK_ERROR(err);
  for (PetscInt faceDM = ffStart; faceDM < ffEnd; ++faceDM, ++firstFaultCell, ++firstFaultCellDM) {
    if (debug) std::cout << "Considering fault face " << faceDM << std::endl;
    const PetscInt *support;
    err = DMPlexGetSupport(faultDMMesh, faceDM, &support);PYLITH_CHECK_ERROR(err);
    // Transform to original mesh cells
    point_type cell      = subpointMap[support[0]];
    point_type otherCell = subpointMap[support[1]];

    if (debug) std::cout << "  Checking orientation against cell " << cell << std::endl;
    PetscInt *faceConeDM = NULL, closureSize, coneSizeDM = 0;
    err = DMPlexGetTransitiveClosure(faultDMMesh, faceDM, PETSC_TRUE, &closureSize, &faceConeDM);PYLITH_CHECK_ERROR(err);
    for (PetscInt c = 0; c < closureSize*2; c += 2) {
      if ((faceConeDM[c] >= fvtStart) && (faceConeDM[c] < fvtEnd)) {
        // TODO After everything works, I will reform the subpointMap with new original vertices
        faceConeDM[coneSizeDM++] = subpointMap[faceConeDM[c]];
      }
    }
    int coneSize;
    bool found = true;

    err = DMPlexGetOrientedFace(complexMesh, cell, coneSizeDM, faceConeDM, numCornersDM, indicesDM, origVerticesDM, faceVerticesDM, NULL);PYLITH_CHECK_ERROR(err);
    if (numFaultCorners == 0) {
      found = false;
    } else if (numFaultCorners == 2) {
      if (faceVerticesDM[0] != faceConeDM[0])
        found = false;
    } else {
      int v = 0;
      // Locate first vertex
      while((v < numFaultCorners) && (faceVerticesDM[v] != faceConeDM[0]))
        ++v;
      for(int c = 0; c < coneSizeDM; ++c, ++v) {
        if (debug) std::cout << "    Checking " << faceConeDM[c] << " against " << faceVerticesDM[v%numFaultCorners] << std::endl;
        if (faceVerticesDM[v%numFaultCorners] != faceConeDM[c]) {
          found = false;
          break;
        } // if
      } // for
    } // if/else

    if (found) {
      if (debug) std::cout << "  Choosing other cell" << std::endl;
      point_type tmpCell = otherCell;
      otherCell = cell;
      cell = tmpCell;
    } else {
      if (debug) std::cout << "  Verifing reverse orientation" << std::endl;
      found = true;
      int v = 0;
      if (numFaultCorners > 0) {
        // Locate first vertex
        while((v < numFaultCorners) && (faceVerticesDM[v] != faceConeDM[coneSizeDM-1]))
          ++v;
        for(int c = coneSizeDM-1; c >= 0; --c, ++v) {
          if (debug) std::cout << "    Checking " << faceConeDM[c] << " against " << faceVerticesDM[v%numFaultCorners] << std::endl;
          if (faceVerticesDM[v%numFaultCorners] != faceConeDM[c]) {
            found = false;
            break;
          } // if
        } // for
      } // if
      if (!found) {
        std::cout << "Considering fault face " << faceDM << std::endl;
        std::cout << "  bordered by cells " << cell << " and " << otherCell << std::endl;
        for(int c = 0; c < coneSizeDM; ++c) {
          std::cout << "    Checking " << faceConeDM[c] << " against " << faceVerticesDM[c] << std::endl;
        } // for
      } // if
      assert(found);
    } // else
    noReplaceCells.insert(otherCell);
    replaceCells.insert(cell);
    replaceVerticesDM.insert(faceConeDM, &faceConeDM[coneSizeDM]);
    cellRenumber[cell]   = firstFaultCell;
    cellRenumberDM[cell] = firstFaultCellDM;
    // Adding cohesive cell (not interpolated)
    PetscInt newv = 0;
    if (debug) std::cout << "  Creating cohesive cell " << firstFaultCell << std::endl;
    for (int c = 0; c < coneSizeDM; ++c) {
      if (debug) std::cout << "    vertex " << faceConeDM[c] << std::endl;
      cohesiveCone[newv++] = faceConeDM[c] + extraCells;
    } // for
    for (int c = 0; c < coneSizeDM; ++c) {
      if (debug) std::cout << "    shadow vertex " << vertexRenumberDM[faceConeDM[c] + extraCells] << std::endl;
      cohesiveCone[newv++] = vertexRenumberDM[faceConeDM[c] + extraCells];
    } // for
    if (constraintCell) {
      for (int c = 0; c < coneSizeDM; ++c) {
        if (debug) std::cout << "    Lagrange vertex " << vertexLagrangeRenumberDM[faceConeDM[c] + extraCells] << std::endl;
        cohesiveCone[newv++] = vertexLagrangeRenumberDM[faceConeDM[c] + extraCells];
      } // for
    } // if
    err = DMPlexSetCone(newMesh, firstFaultCellDM, cohesiveCone);PYLITH_CHECK_ERROR(err);
    err = DMPlexSetLabelValue(newMesh, "material-id", firstFaultCellDM, materialId);PYLITH_CHECK_ERROR(err);
    err = DMPlexRestoreTransitiveClosure(faultDMMesh, faceDM, PETSC_TRUE, &closureSize, &faceConeDM);PYLITH_CHECK_ERROR(err);
  } // for over fault faces
  // This completes the set of cells scheduled to be replaced
  TopologyOps::PointSet faultBdVertices;
  IS              bdSubpointIS;
  const PetscInt *points;
  PetscInt        bfvStart, bfvEnd;

  assert(faultBoundary);
  err = DMPlexGetDepthStratum(faultBoundary, 0, &bfvStart, &bfvEnd);PYLITH_CHECK_ERROR(err);
  err = DMPlexCreateSubpointIS(faultBoundary, &bdSubpointIS);PYLITH_CHECK_ERROR(err);
  if (bdSubpointIS) {err = ISGetIndices(bdSubpointIS, &points);PYLITH_CHECK_ERROR(err);}
  for (PetscInt v = bfvStart; v < bfvEnd; ++v) {
    faultBdVertices.insert(subpointMap[points[v]]);
  }
  if (bdSubpointIS) {err = ISRestoreIndices(bdSubpointIS, &points);PYLITH_CHECK_ERROR(err);}
  err = ISDestroy(&bdSubpointIS);PYLITH_CHECK_ERROR(err);
  if (subpointIS) {err = ISRestoreIndices(subpointIS, &subpointMap);PYLITH_CHECK_ERROR(err);}
  err = ISDestroy(&subpointIS);PYLITH_CHECK_ERROR(err);
  // Classify cells by side of the fault
  TopologyOps::PointSet::const_iterator rVerticesEnd = replaceVerticesDM.end();
  for (TopologyOps::PointSet::const_iterator v_iter = replaceVerticesDM.begin(); v_iter != rVerticesEnd; ++v_iter) {
    if (faultBdVertices.find(*v_iter) != faultBdVertices.end())
      continue;
    TopologyOps::classifyCellsDM(complexMesh, *v_iter, depth, faceSizeDM, firstCohesiveCellDM, replaceCells, noReplaceCells, debug);
  } // for
  const TopologyOps::PointSet::const_iterator fbdVerticesEnd = faultBdVertices.end();
  for (TopologyOps::PointSet::const_iterator v_iter = faultBdVertices.begin(); v_iter != fbdVerticesEnd; ++v_iter) {
    TopologyOps::classifyCellsDM(complexMesh, *v_iter, depth, faceSizeDM, firstCohesiveCellDM, replaceCells, noReplaceCells, debug);
  } // for
  // Insert replaced vertices into cones (could use DMPlexInsertCone())
  TopologyOps::PointSet::const_iterator rVerticesDMEnd = replaceVerticesDM.end();
  for(PetscInt cell = cStart; cell < cEnd; ++cell) {
    const PetscInt *cone;
    PetscInt        coneSize;

    err = DMPlexGetCone(complexMesh, cell, &cone);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetConeSize(complexMesh, cell, &coneSize);PYLITH_CHECK_ERROR(err);
    if (replaceCells.find(cell) != replaceCells.end()) {
      for(PetscInt c = 0; c < coneSize; ++c) {
        PetscBool replaced = PETSC_FALSE;

        for(TopologyOps::PointSet::const_iterator v_iter = replaceVerticesDM.begin(); v_iter != rVerticesDMEnd; ++v_iter) {
          if (cone[c] == *v_iter) {
            cohesiveCone[c] = vertexRenumberDM[cone[c] + extraCells];
            replaced        = PETSC_TRUE;
            break;
          }
        }
        if (!replaced) {
          cohesiveCone[c] = cone[c] + extraCells;
        }
      }
    } else {
      for(PetscInt c = 0; c < coneSize; ++c) {
        cohesiveCone[c] = cone[c] + extraCells;
      }
    }
    err = DMPlexSetCone(newMesh, cell, cohesiveCone);PYLITH_CHECK_ERROR(err);
  }
  err = PetscFree3(origVerticesDM, faceVerticesDM, cohesiveCone);PYLITH_CHECK_ERROR(err);
  /* DMPlex */
  err = DMPlexSymmetrize(newMesh);PYLITH_CHECK_ERROR(err);
  err = DMPlexStratify(newMesh);PYLITH_CHECK_ERROR(err);

  if (!rank) {
    err = PetscFree(indicesDM);PYLITH_CHECK_ERROR(err);
  }

  // Fix coordinates
  PetscSection coordSection, newCoordSection;
  Vec          coordinatesVec, newCoordinatesVec;
  PetscScalar *coords, *newCoords;
  PetscInt     numComp = 0, coordSize = 0;
 
  err = DMGetCoordinateSection(complexMesh, &coordSection);PYLITH_CHECK_ERROR(err);
  err = DMGetCoordinateSection(newMesh,     &newCoordSection);PYLITH_CHECK_ERROR(err);
  err = PetscSectionSetNumFields(newCoordSection, 1);PYLITH_CHECK_ERROR(err);
  if (vEnd > vStart) {err = PetscSectionGetDof(coordSection, vStart, &numComp);PYLITH_CHECK_ERROR(err);}
  err = PetscSectionSetFieldComponents(newCoordSection, 0, numComp);PYLITH_CHECK_ERROR(err);
  err = DMGetCoordinatesLocal(complexMesh, &coordinatesVec);PYLITH_CHECK_ERROR(err);
  err = PetscSectionSetChart(newCoordSection, vStart+extraCells, vEnd+extraCells+extraVertices);PYLITH_CHECK_ERROR(err);
  for(PetscInt v = vStart; v < vEnd; ++v) {
    PetscInt dof;
    err = PetscSectionGetDof(coordSection, v, &dof);PYLITH_CHECK_ERROR(err);
    err = PetscSectionSetDof(newCoordSection, v+extraCells, dof);PYLITH_CHECK_ERROR(err);
    err = PetscSectionSetFieldDof(newCoordSection, v+extraCells, 0, dof);PYLITH_CHECK_ERROR(err);
  }

  for (PetscInt fv = 0; fv < numFaultVerticesDM; ++fv) {
    PetscInt v    = fVerticesDM[fv];
    PetscInt vnew = v+extraCells;
    PetscInt dof;

    err = PetscSectionGetDof(coordSection, v, &dof);PYLITH_CHECK_ERROR(err);
    err = PetscSectionSetDof(newCoordSection, vertexRenumberDM[vnew], dof);PYLITH_CHECK_ERROR(err);
    err = PetscSectionSetFieldDof(newCoordSection, vertexRenumberDM[vnew], 0, dof);PYLITH_CHECK_ERROR(err);
    if (constraintCell) {
      err = PetscSectionSetDof(newCoordSection, vertexLagrangeRenumberDM[vnew], dof);PYLITH_CHECK_ERROR(err);
      err = PetscSectionSetFieldDof(newCoordSection, vertexLagrangeRenumberDM[vnew], 0, dof);PYLITH_CHECK_ERROR(err);
    }
  } // for
  err = PetscSectionSetUp(newCoordSection);PYLITH_CHECK_ERROR(err);
  err = PetscSectionGetStorageSize(newCoordSection, &coordSize);PYLITH_CHECK_ERROR(err);
  err = VecCreate(mesh->comm(), &newCoordinatesVec);PYLITH_CHECK_ERROR(err);
  err = VecSetSizes(newCoordinatesVec, coordSize, PETSC_DETERMINE);PYLITH_CHECK_ERROR(err);
  err = VecSetFromOptions(newCoordinatesVec);PYLITH_CHECK_ERROR(err);
  err = VecGetArray(coordinatesVec, &coords);PYLITH_CHECK_ERROR(err);
  err = VecGetArray(newCoordinatesVec, &newCoords);PYLITH_CHECK_ERROR(err);

  for(PetscInt v = vStart; v < vEnd; ++v) {
    PetscInt vnew = v+extraCells;
    PetscInt dof, off, newoff, d;

    err = PetscSectionGetDof(coordSection, v, &dof);PYLITH_CHECK_ERROR(err);
    err = PetscSectionGetOffset(coordSection, v, &off);PYLITH_CHECK_ERROR(err);
    err = PetscSectionGetOffset(newCoordSection, vnew, &newoff);PYLITH_CHECK_ERROR(err);
    for(PetscInt d = 0; d < dof; ++d) {
      newCoords[newoff+d] = coords[off+d];
    }
  }
  for (PetscInt fv = 0; fv < numFaultVerticesDM; ++fv) {
    PetscInt v    = fVerticesDM[fv];
    PetscInt vnew = v+extraCells;
    PetscInt dof, off, newoff, d;

    err = PetscSectionGetDof(coordSection, v, &dof);PYLITH_CHECK_ERROR(err);
    err = PetscSectionGetOffset(coordSection, v, &off);PYLITH_CHECK_ERROR(err);
    err = PetscSectionGetOffset(newCoordSection, vertexRenumberDM[vnew], &newoff);PYLITH_CHECK_ERROR(err);
    for(PetscInt d = 0; d < dof; ++d) {
      newCoords[newoff+d] = coords[off+d];
    }
    if (constraintCell) {
      err = PetscSectionGetOffset(newCoordSection, vertexLagrangeRenumberDM[vnew], &newoff);PYLITH_CHECK_ERROR(err);
      for(PetscInt d = 0; d < dof; ++d) {
        newCoords[newoff+d] = coords[off+d];
      }
    } // if
  } // for
  err = VecRestoreArray(coordinatesVec, &coords);PYLITH_CHECK_ERROR(err);
  err = VecRestoreArray(newCoordinatesVec, &newCoords);PYLITH_CHECK_ERROR(err);
  err = DMSetCoordinatesLocal(newMesh, newCoordinatesVec);PYLITH_CHECK_ERROR(err);
  err = VecDestroy(&newCoordinatesVec);PYLITH_CHECK_ERROR(err);

  if (fVertexIS) {err = ISRestoreIndices(fVertexIS, &fVerticesDM);PYLITH_CHECK_ERROR(err);}
  err = ISDestroy(&fVertexIS);PYLITH_CHECK_ERROR(err);

  PetscReal lengthScale = 1.0;
  err = DMPlexGetScale(complexMesh, PETSC_UNIT_LENGTH, &lengthScale);PYLITH_CHECK_ERROR(err);
  err = DMPlexSetScale(newMesh, PETSC_UNIT_LENGTH, lengthScale);PYLITH_CHECK_ERROR(err);
  mesh->dmMesh(newMesh);

  PYLITH_METHOD_END;
} // create

void
pylith::faults::CohesiveTopology::createInterpolated(topology::Mesh* mesh,
						     const topology::Mesh& faultMesh,
						     PetscDM faultBoundary,
						     const int materialId,
						     int& firstFaultVertex,
						     int& firstLagrangeVertex,
						     int& firstFaultCell,
						     const bool constraintCell)
{ // createInterpolated
  assert(mesh);
  assert(faultBoundary);
  PetscDM        sdm = NULL;
  PetscDM        dm  = mesh->dmMesh();assert(dm);
  PetscDMLabel   subpointMap = NULL, label = NULL, mlabel = NULL;
  PetscInt       dim, cMax, cEnd, numCohesiveCellsOld;
  PetscErrorCode err;

  // Have to remember the old number of cohesive cells
  err = DMPlexGetHeightStratum(dm, 0, NULL, &cEnd);PYLITH_CHECK_ERROR(err);
  err = DMPlexGetHybridBounds(dm, &cMax, NULL, NULL, NULL);PYLITH_CHECK_ERROR(err);
  numCohesiveCellsOld = cEnd - (cMax < 0 ? cEnd : cMax);
  // Create cohesive cells
  err = DMPlexGetSubpointMap(faultMesh.dmMesh(), &subpointMap);PYLITH_CHECK_ERROR(err);
  err = DMLabelDuplicate(subpointMap, &label);PYLITH_CHECK_ERROR(err);
  err = DMLabelClearStratum(label, mesh->dimension());PYLITH_CHECK_ERROR(err);
  // Completes the set of cells scheduled to be replaced
  //   Have to do internal fault vertices before fault boundary vertices, and this is the only thing I use faultBoundary for
  err = DMPlexLabelCohesiveComplete(dm, label, PETSC_FALSE, faultMesh.dmMesh());PYLITH_CHECK_ERROR(err);
  err = DMPlexConstructCohesiveCells(dm, label, &sdm);PYLITH_CHECK_ERROR(err);

  /* TODO: Eliminate Lagrange dofs from lopback edges */
  err = DMPlexGetDimension(dm, &dim);PYLITH_CHECK_ERROR(err);
  err = DMPlexGetLabel(sdm, "material-id", &mlabel);PYLITH_CHECK_ERROR(err);
  if (mlabel) {
    err = DMPlexGetHeightStratum(sdm, 0, NULL, &cEnd);PYLITH_CHECK_ERROR(err);
    err = DMPlexGetHybridBounds(sdm, &cMax, NULL, NULL, NULL);PYLITH_CHECK_ERROR(err);
    assert(cEnd > cMax + numCohesiveCellsOld);
    for (PetscInt cell = cMax; cell < cEnd - numCohesiveCellsOld; ++cell) {
      PetscInt onBd;

      /* Eliminate hybrid cells on the boundary of the split from cohesive label,
         they are marked with -(cell number) since the hybrid cell number aliases vertices in the old mesh */
      err = DMLabelGetValue(label, -cell, &onBd);PYLITH_CHECK_ERROR(err);
      if (onBd == dim) continue;
      err = DMLabelSetValue(mlabel, cell, materialId);PYLITH_CHECK_ERROR(err);
    }
  }
  err = DMLabelDestroy(&label);PYLITH_CHECK_ERROR(err);

  PetscReal lengthScale = 1.0;
  err = DMPlexGetScale(dm, PETSC_UNIT_LENGTH, &lengthScale);PYLITH_CHECK_ERROR(err);
  err = DMPlexSetScale(sdm, PETSC_UNIT_LENGTH, lengthScale);PYLITH_CHECK_ERROR(err);
  mesh->dmMesh(sdm);
} // createInterpolated

// ----------------------------------------------------------------------
// Form a parallel fault mesh using the cohesive cell information
void
pylith::faults::CohesiveTopology::createFaultParallel(topology::Mesh* faultMesh,
						      const topology::Mesh& mesh,
						      const int materialId,
						      const char* label,
						      const bool constraintCell)
{ // createFaultParallel
  PYLITH_METHOD_BEGIN;

  assert(faultMesh);
  const char    *labelname = "material-id";
  PetscErrorCode err;

  faultMesh->coordsys(mesh.coordsys());

  PetscDM dmMesh = mesh.dmMesh();assert(dmMesh);
  PetscDM dmFaultMesh;


  err = DMPlexCreateCohesiveSubmesh(dmMesh, constraintCell ? PETSC_TRUE : PETSC_FALSE, labelname, materialId, &dmFaultMesh);PYLITH_CHECK_ERROR(err);
  err = DMPlexOrient(dmFaultMesh);PYLITH_CHECK_ERROR(err);
  std::string meshLabel = "fault_" + std::string(label);

  PetscReal lengthScale = 1.0;
  err = DMPlexGetScale(dmMesh, PETSC_UNIT_LENGTH, &lengthScale);PYLITH_CHECK_ERROR(err);
  err = DMPlexSetScale(dmFaultMesh, PETSC_UNIT_LENGTH, lengthScale);PYLITH_CHECK_ERROR(err);

  faultMesh->dmMesh(dmFaultMesh, meshLabel.c_str());

  PYLITH_METHOD_END;
} // createFaultParallel


// End of file
