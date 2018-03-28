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
// Copyright (c) 2010-2015 University of California, Davis
//
// See COPYING for license information.
//
// ----------------------------------------------------------------------
//

#include <portinfo>

#include "TestMaterial.hh" // Implementation of class methods

#include "pylith/materials/Material.hh" // USES Material
#include "pylith/materials/Query.hh" // USES Query

#include "pylith/topology/Mesh.hh" // USES Mesh
#include "pylith/topology/MeshOps.hh" // USES MeshOps::nondimensionalize()
#include "pylith/topology/Field.hh" // USES Field
#include "pylith/topology/Fields.hh" // USES Fields
#include "pylith/topology/VisitorMesh.hh" // USES VecVisitorMesh
#include "pylith/topology/FieldQuery.hh" // USES FieldQuery
#include "pylith/feassemble/AuxiliaryFactory.hh" // USES AuxiliaryFactory
#include "pylith/meshio/MeshIOAscii.hh" // USES MeshIOAscii
#include "pylith/utils/error.hh" // USES PYLITH_METHOD_BEGIN/END

#include "spatialdata/spatialdb/UserFunctionDB.hh" // USES UserFunctionDB
#include "spatialdata/geocoords/CoordSys.hh" // USES CoordSys
#include "spatialdata/units/Nondimensional.hh" // USES Nondimensional

#include "journal/debug.h" // USES journal::debug_t

// ----------------------------------------------------------------------
// Setup testing data.
void
pylith::materials::TestMaterial::setUp(void)
{ // setUp
    _mesh = new pylith::topology::Mesh();CPPUNIT_ASSERT(_mesh);
    _solutionFields = NULL;
} // setUp


// ----------------------------------------------------------------------
// Deallocate testing data.
void
pylith::materials::TestMaterial::tearDown(void)
{ // tearDown
    delete _solutionFields; _solutionFields = NULL;
    delete _mesh; _mesh = NULL;
} // tearDown


// ----------------------------------------------------------------------
// Test auxField().
void
pylith::materials::TestMaterial::testAuxField(void)
{ // testAuxField
    PYLITH_METHOD_BEGIN;

    _initializeFull();

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);

    const pylith::topology::Field& auxField = material->auxField();
    for (int i = 0; i < data->numAuxSubfields; ++i) {
        CPPUNIT_ASSERT(auxField.hasSubfield(data->auxSubfields[i]));
    } // for

    CPPUNIT_ASSERT(!auxField.hasSubfield("abc4598245"));

    PYLITH_METHOD_END;
} // testAuxField


// ----------------------------------------------------------------------
// Test auxSubfieldDiscretization().
void
pylith::materials::TestMaterial::testAuxSubfieldDiscretization(void)
{ // testAuxSubfieldDiscretization
    PYLITH_METHOD_BEGIN;

    const topology::FieldBase::Discretization infoDefault = {-1, -1, true, pylith::topology::FieldBase::POLYNOMIAL_SPACE};
    const topology::FieldBase::Discretization infoA = {1, 2, false, pylith::topology::FieldBase::POLYNOMIAL_SPACE};
    const topology::FieldBase::Discretization infoB = {2, 2, true, pylith::topology::FieldBase::POINT_SPACE};

    Material* material = _material(); CPPUNIT_ASSERT(material);
    material->auxSubfieldDiscretization("A", infoA.basisOrder, infoA.quadOrder, infoA.isBasisContinuous, infoA.feSpace);
    material->auxSubfieldDiscretization("B", infoB.basisOrder, infoB.quadOrder, infoB.isBasisContinuous, infoB.feSpace);

    CPPUNIT_ASSERT(material->_auxFactory());
    { // A
        const topology::FieldBase::Discretization& test = material->_auxFactory()->subfieldDiscretization("A");
        CPPUNIT_ASSERT_EQUAL(infoA.basisOrder, test.basisOrder);
        CPPUNIT_ASSERT_EQUAL(infoA.quadOrder, test.quadOrder);
        CPPUNIT_ASSERT_EQUAL(infoA.isBasisContinuous, test.isBasisContinuous);
        CPPUNIT_ASSERT_EQUAL(infoA.feSpace, test.feSpace);
    } // A

    { // B
        const topology::FieldBase::Discretization& test = material->_auxFactory()->subfieldDiscretization("B");
        CPPUNIT_ASSERT_EQUAL(infoB.basisOrder, test.basisOrder);
        CPPUNIT_ASSERT_EQUAL(infoB.quadOrder, test.quadOrder);
        CPPUNIT_ASSERT_EQUAL(infoB.isBasisContinuous, test.isBasisContinuous);
        CPPUNIT_ASSERT_EQUAL(infoB.feSpace, test.feSpace);
    } // B

    { // C (default)
        const topology::FieldBase::Discretization& test = material->_auxFactory()->subfieldDiscretization("C");
        CPPUNIT_ASSERT_EQUAL(infoDefault.basisOrder, test.basisOrder);
        CPPUNIT_ASSERT_EQUAL(infoDefault.quadOrder, test.quadOrder);
        CPPUNIT_ASSERT_EQUAL(infoDefault.isBasisContinuous, test.isBasisContinuous);
        CPPUNIT_ASSERT_EQUAL(infoDefault.feSpace, test.feSpace);
    } // C (default)

    { // default
        const topology::FieldBase::Discretization& test = material->_auxFactory()->subfieldDiscretization("default");
        CPPUNIT_ASSERT_EQUAL(infoDefault.basisOrder, test.basisOrder);
        CPPUNIT_ASSERT_EQUAL(infoDefault.quadOrder, test.quadOrder);
        CPPUNIT_ASSERT_EQUAL(infoDefault.isBasisContinuous, test.isBasisContinuous);
        CPPUNIT_ASSERT_EQUAL(infoDefault.feSpace, test.feSpace);
    } // default

    PYLITH_METHOD_END;
} // testAuxSubfieldDiscretization


// ----------------------------------------------------------------------
// Test auxFieldDB().
void
pylith::materials::TestMaterial::testAuxFieldDB(void)
{ // testAuxFieldDB
    PYLITH_METHOD_BEGIN;

    const std::string label = "test db";
    spatialdata::spatialdb::UserFunctionDB db;
    db.label(label.c_str());

    Material* material = _material(); CPPUNIT_ASSERT(material);
    material->auxFieldDB(&db);

    CPPUNIT_ASSERT(material->_auxFactory());
    CPPUNIT_ASSERT(material->_auxFactory()->queryDB());
    CPPUNIT_ASSERT_EQUAL(label, std::string(material->_auxFactory()->queryDB()->label()));

    PYLITH_METHOD_END;
} // testAuxFieldDB


// ----------------------------------------------------------------------
// Test normalizer().
void
pylith::materials::TestMaterial::testNormalizer(void)
{ // testNormalizer
    PYLITH_METHOD_BEGIN;

    spatialdata::units::Nondimensional normalizer;
    const double scale = 5.0;
    normalizer.lengthScale(scale);

    Material* material = _material(); CPPUNIT_ASSERT(material);
    material->normalizer(normalizer);
    CPPUNIT_ASSERT_EQUAL(scale, material->_normalizer->lengthScale());

    PYLITH_METHOD_END;
} // testNormalizer


// ----------------------------------------------------------------------
// Test verifyConfiguration().
void
pylith::materials::TestMaterial::testVerifyConfiguration(void)
{ // testVerifyConfiguration
    PYLITH_METHOD_BEGIN;

    // Call verifyConfiguration()
    Material* material = _material(); CPPUNIT_ASSERT(material);
    CPPUNIT_ASSERT(_solutionFields);
    material->verifyConfiguration(_solutionFields->get("solution"));

    // Nothing to test.

    PYLITH_METHOD_END;
} // testVerifyConfiguration


// ----------------------------------------------------------------------
// Test dimension().
void
pylith::materials::TestMaterial::testDimension(void)
{ // testDimension
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);
    CPPUNIT_ASSERT_EQUAL(data->dimension, material->dimension());

    PYLITH_METHOD_END;
} // testDimension


// ----------------------------------------------------------------------
// Test id().
void
pylith::materials::TestMaterial::testId(void)
{ // testId
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);

    const int matId = 1234;
    material->id(matId);
    CPPUNIT_ASSERT_EQUAL(matId, material->id());

    PYLITH_METHOD_END;
} // testId


// ----------------------------------------------------------------------
// Test label().
void
pylith::materials::TestMaterial::testLabel(void)
{ // testLabel
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);
    const std::string& matLabel = "xyz";
    material->label(matLabel.c_str());
    CPPUNIT_ASSERT_EQUAL(matLabel, std::string(material->label()));

    PYLITH_METHOD_END;
} // testLabel


// ----------------------------------------------------------------------
// Test initialize().
void
pylith::materials::TestMaterial::testInitialize(void)
{ // testInitialize
    PYLITH_METHOD_BEGIN;

    // Call initialize()
    _initializeFull(); // includes setting up auxField

    Material* material = _material(); CPPUNIT_ASSERT(material);
    const pylith::topology::Field& auxField = material->auxField();

    //material->_auxField->view("AUX FIELDS"); // :DEBUGGING:

    // Check result
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);
    CPPUNIT_ASSERT_EQUAL(std::string("auxiliary subfields"), std::string(auxField.label()));
    CPPUNIT_ASSERT_EQUAL(data->dimension, auxField.spaceDim());

    PylithReal norm = 0.0;
    PylithReal t = 0.0;
    const PetscDM dm = auxField.dmMesh(); CPPUNIT_ASSERT(dm);
    pylith::topology::FieldQuery query(auxField);
    query.initializeWithDefaultQueryFns();
    CPPUNIT_ASSERT(data->normalizer);
    query.openDB(data->auxDB, data->normalizer->lengthScale());
    PetscErrorCode err = DMPlexComputeL2DiffLocal(dm, t, query.functions(), (void**)query.contextPtrs(), auxField.localVector(), &norm); CPPUNIT_ASSERT(!err);
    query.closeDB(data->auxDB);
    const PylithReal tolerance = 1.0e-6;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, norm, tolerance);

    PYLITH_METHOD_END;
} // testInitialize


// ----------------------------------------------------------------------
// Test computeResidual().
void
pylith::materials::TestMaterial::testComputeResidual(void)
{ // testComputeResidual
    PYLITH_METHOD_BEGIN;

    // Call initialize()
    _initializeFull(); // includes setting up auxField

    CPPUNIT_ASSERT(_mesh);
    CPPUNIT_ASSERT(_solutionFields);
    pylith::topology::Field& solution = _solutionFields->get("solution");
    pylith::topology::Field& solutionDot = _solutionFields->get("solution_dot");

    pylith::topology::Field residualRHS(*_mesh);
    residualRHS.cloneSection(solution);
    residualRHS.label("residual RHS");
    residualRHS.allocate();

    pylith::topology::Field residualLHS(*_mesh);
    residualLHS.cloneSection(solution);
    residualLHS.label("residual LHS");
    residualLHS.allocate();

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);

#if 0 // :DEBUG:
    PetscOptionsSetValue(NULL, "-dm_plex_print_fem", "2"); // :DEBUG:
    DMSetFromOptions(residualRHS.dmMesh()); // :DEBUG:
#endif // :DEBUG:

    const PylithReal t = data->t;
    const PylithReal dt = data->dt;
    material->computeRHSResidual(&residualRHS, t, dt, solution);
    material->computeLHSResidual(&residualLHS, t, dt, solution, solutionDot);

    // We don't use Dirichlet BC, so we must manually zero out the residual values for constrained DOF.
    _zeroBoundary(&residualRHS);
    _zeroBoundary(&residualLHS);

#if 0 // :DEBUG:
    solution.view("SOLUTION"); // :DEBUG:
    solutionDot.view("SOLUTION_DOT"); // :DEBUG:
    residualRHS.view("RESIDUAL RHS"); // :DEBUG:
    residualLHS.view("RESIDUAL LHS"); // :DEBUG:
#endif // :DEBUG:

    PetscErrorCode err;
    PetscVec residualVec = NULL;
    err = VecDuplicate(residualRHS.localVector(), &residualVec); CPPUNIT_ASSERT(!err);
    err = VecWAXPY(residualVec, -1.0, residualRHS.localVector(), residualLHS.localVector()); CPPUNIT_ASSERT(!err);

    PylithReal norm = 0.0;
    PylithReal normRHS = 0.0;
    PylithReal normLHS = 0.0;
    err = VecNorm(residualRHS.localVector(), NORM_2, &normRHS); CPPUNIT_ASSERT(!err);
    err = VecNorm(residualLHS.localVector(), NORM_2, &normLHS); CPPUNIT_ASSERT(!err);
    err = VecNorm(residualVec, NORM_2, &norm); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&residualVec); CPPUNIT_ASSERT(!err);
    const PylithReal tolerance = 1.0e-6;
    CPPUNIT_ASSERT(normRHS > 0.0 || normLHS > 0.0); // Avoid trivial satisfaction of norm with zero values.
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, norm, tolerance);

    PYLITH_METHOD_END;
} // testComputeResidual


// ----------------------------------------------------------------------
// Test computeRHSJacobian().
void
pylith::materials::TestMaterial::testComputeRHSJacobian(void)
{ // testComputeRHSJacobian
    PYLITH_METHOD_BEGIN;

    // Create linear problem (MMS) with two trial solutions, s and p.
    //
    // Check that Jg(s)*(p - s) = G(p) - G(s).

    // Call initialize()
    _initializeFull();

    CPPUNIT_ASSERT(_mesh);
    CPPUNIT_ASSERT(_solutionFields);
    pylith::topology::Field& solution = _solutionFields->get("solution");
    pylith::topology::Field& perturbation = _solutionFields->get("perturbation");

    pylith::topology::Field residual1(*_mesh);
    residual1.cloneSection(solution);
    residual1.label("residual1");
    residual1.allocate();

    pylith::topology::Field residual2(*_mesh);
    residual2.cloneSection(perturbation);
    residual2.label("residual2");
    residual2.allocate();

#if 0 // :DEBUG:
    PetscOptionsSetValue(NULL, "-dm_plex_print_fem", "2"); // :DEBUG:
    DMSetFromOptions(_solution1->dmMesh()); // :DEBUG:
#endif // :DEBUG:

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);

    const PylithReal t = data->t;
    const PylithReal dt = data->dt;
    material->computeRHSResidual(&residual1, t, dt, solution);
    material->computeRHSResidual(&residual2, t, dt, perturbation);

    residual1.view("RESIDUAL 1 RHS"); // :DEBUG:
    //residual2.view("RESIDUAL 2 RHS"); // :DEBUG:

    // Compute Jacobian
    PetscErrorCode err;
    PetscMat jacobianMat = NULL;
    err = DMCreateMatrix(solution.dmMesh(), &jacobianMat); CPPUNIT_ASSERT(!err);
    err = MatZeroEntries(jacobianMat); CPPUNIT_ASSERT(!err);
    PetscMat precondMat = jacobianMat; // Use Jacobian == preconditioner

    material->computeRHSJacobian(jacobianMat, precondMat, t, dt, solution);
    CPPUNIT_ASSERT_EQUAL(false, material->needNewRHSJacobian());
    //_zeroBoundary(&residual1);
    //_zeroBoundary(&residual2, jacobianMat);
    err = MatAssemblyBegin(jacobianMat, MAT_FINAL_ASSEMBLY); PYLITH_CHECK_ERROR(err);
    err = MatAssemblyEnd(jacobianMat, MAT_FINAL_ASSEMBLY); PYLITH_CHECK_ERROR(err);

    // Check that J(s)*(p - s) = G(p) - G(s).

    PetscVec residualVec = NULL;
    err = VecDuplicate(residual1.localVector(), &residualVec); CPPUNIT_ASSERT(!err);
    err = VecWAXPY(residualVec, -1.0, residual1.localVector(), residual2.localVector()); CPPUNIT_ASSERT(!err);

    PetscVec solnIncrVec = NULL;
    err = VecDuplicate(solution.localVector(), &solnIncrVec); CPPUNIT_ASSERT(!err);
    err = VecWAXPY(solnIncrVec, -1.0, solution.localVector(), perturbation.localVector()); CPPUNIT_ASSERT(!err);


    // result = Jg*(-solnIncr) + residual
    PetscVec resultVec = NULL;
    err = VecDuplicate(residualVec, &resultVec); CPPUNIT_ASSERT(!err);
    err = VecZeroEntries(resultVec); CPPUNIT_ASSERT(!err);
    err = VecScale(solnIncrVec, -1.0); CPPUNIT_ASSERT(!err);
    err = MatMultAdd(jacobianMat, solnIncrVec, residualVec, resultVec); CPPUNIT_ASSERT(!err);

#if 1 // :DEBUG:
    std::cout << "SOLN INCR" << std::endl;
    VecView(solnIncrVec, PETSC_VIEWER_STDOUT_SELF);
    std::cout << "G2-G1" << std::endl;
    VecView(residualVec, PETSC_VIEWER_STDOUT_SELF);
    std::cout << "RESULT" << std::endl;
    VecView(resultVec, PETSC_VIEWER_STDOUT_SELF);
#endif // :DEBUG:

    PylithReal norm = 0.0;
    err = VecNorm(resultVec, NORM_2, &norm); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&resultVec); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&solnIncrVec); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&residualVec); CPPUNIT_ASSERT(!err);
    err = MatDestroy(&jacobianMat); CPPUNIT_ASSERT(!err);

    const PylithReal tolerance = 1.0e-6;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, norm, tolerance);
    CPPUNIT_ASSERT(norm > 0.0); // Norm exactly equal to zero almost certainly means test is satisfied trivially.

    PYLITH_METHOD_END;
} // testComputeRHSJacobian


// ----------------------------------------------------------------------
// Test computeLHSJacobianImplicit().
void
pylith::materials::TestMaterial::testComputeLHSJacobianImplicit(void)
{ // testComputeLHSJacobianImplicit
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);
    const TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);
    if (data->isExplicit) {
        PYLITH_METHOD_END;
    } // if

    // Create linear problem (MMS) with two trial solutions, s,s_dor and p,p_dot.
    //
    // Check that Jf(s,s_dot)*(p - s) = F(p,p_dot) - F(s,s_dot).

    // Call initialize()
    _initializeFull(); // includes setting up auxField

    CPPUNIT_ASSERT(_mesh);
    CPPUNIT_ASSERT(_solutionFields);
    pylith::topology::Field& solution = _solutionFields->get("solution");
    pylith::topology::Field& solutionDot = _solutionFields->get("solution_dot");
    pylith::topology::Field& perturbation = _solutionFields->get("perturbation");
    pylith::topology::Field& perturbationDot = _solutionFields->get("perturbation_dot");

    pylith::topology::Field residual1(*_mesh);
    residual1.cloneSection(solution);
    residual1.label("residual1");
    residual1.allocate();

    pylith::topology::Field residual2(*_mesh);
    residual2.cloneSection(perturbation);
    residual2.label("residual2");
    residual2.allocate();

#if 0 // :DEBUG:
    PetscOptionsSetValue(NULL, "-dm_plex_print_fem", "2"); // :DEBUG:
    DMSetFromOptions(_solution1->dmMesh()); // :DEBUG:
#endif // :DEBUG:


    const PylithReal t = data->t;
    const PylithReal dt = data->dt;
    const PylithReal tshift = data->tshift;
    material->computeLHSResidual(&residual1, t, dt, solution, solutionDot);
    material->computeLHSResidual(&residual2, t, dt, perturbation, perturbationDot);

    //residual1.view("RESIDUAL 1 LHS"); // :DEBUG:
    //residual2.view("RESIDUAL 2 LHS"); // :DEBUG:

    PetscErrorCode err;

    PetscVec residualVec = NULL;
    err = VecDuplicate(residual1.localVector(), &residualVec); CPPUNIT_ASSERT(!err);
    err = VecWAXPY(residualVec, -1.0, residual1.localVector(), residual2.localVector()); CPPUNIT_ASSERT(!err);

    PetscVec solnIncrVec = NULL;
    err = VecDuplicate(solution.localVector(), &solnIncrVec); CPPUNIT_ASSERT(!err);
    err = VecWAXPY(solnIncrVec, -1.0, solution.localVector(), perturbation.localVector()); CPPUNIT_ASSERT(!err);

    // Compute Jacobian
    PetscMat jacobianMat = NULL;
    err = DMCreateMatrix(solution.dmMesh(), &jacobianMat); CPPUNIT_ASSERT(!err);
    err = MatZeroEntries(jacobianMat); CPPUNIT_ASSERT(!err);
    PetscMat precondMat = jacobianMat; // Use Jacobian == preconditioner

    material->computeLHSJacobianImplicit(jacobianMat, precondMat, t, dt, tshift, solution, solutionDot);
    CPPUNIT_ASSERT_EQUAL(false, material->needNewLHSJacobian());
    err = MatAssemblyBegin(jacobianMat, MAT_FINAL_ASSEMBLY); PYLITH_CHECK_ERROR(err);
    err = MatAssemblyEnd(jacobianMat, MAT_FINAL_ASSEMBLY); PYLITH_CHECK_ERROR(err);

    // result = J*(-solnIncr) + residual
    PetscVec resultVec = NULL;
    err = VecDuplicate(residualVec, &resultVec); CPPUNIT_ASSERT(!err);
    err = VecZeroEntries(resultVec); CPPUNIT_ASSERT(!err);
    err = VecScale(solnIncrVec, -1.0); CPPUNIT_ASSERT(!err);
    err = MatMultAdd(jacobianMat, solnIncrVec, residualVec, resultVec); CPPUNIT_ASSERT(!err);

#if 0 // :DEBUG:
    std::cout << "SOLN INCR" << std::endl;
    VecView(solnIncrVec, PETSC_VIEWER_STDOUT_SELF);
    std::cout << "F2-F1" << std::endl;
    VecView(residualVec, PETSC_VIEWER_STDOUT_SELF);
    std::cout << "RESULT" << std::endl;
    VecView(resultVec, PETSC_VIEWER_STDOUT_SELF);
#endif // :DEBUG:

    PylithReal norm = 0.0, normSolnIncr = 0.0, normResidual = 0.0;
    err = VecNorm(resultVec, NORM_2, &norm); CPPUNIT_ASSERT(!err);
    err = VecNorm(solnIncrVec, NORM_2, &normSolnIncr); CPPUNIT_ASSERT(!err);
    err = VecNorm(residualVec, NORM_2, &normResidual); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&resultVec); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&solnIncrVec); CPPUNIT_ASSERT(!err);
    err = VecDestroy(&residualVec); CPPUNIT_ASSERT(!err);
    err = MatDestroy(&jacobianMat); CPPUNIT_ASSERT(!err);

    const PylithReal tolerance = 1.0e-6;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, norm, tolerance);
    CPPUNIT_ASSERT((0 < normResidual && 0 < norm) || (0 == normResidual && 0 == norm));

    PYLITH_METHOD_END;
} // testComputeLHSJacobianImplicit


// ----------------------------------------------------------------------
// Test computeLHSJacobianExplicit().
void
pylith::materials::TestMaterial::testComputeLHSJacobianInverseExplicit(void)
{ // testComputeLHSJacobianInverseExplicit
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);
    if (!data->isExplicit) {
        PYLITH_METHOD_END;
    } // if

    CPPUNIT_ASSERT_MESSAGE("Test not implemented.", false); // :TODO: ADD MORE HERE

    PYLITH_METHOD_END;
} // testComputeLHSJacobianInverseExplicit


// ----------------------------------------------------------------------
// Test updateStateVars().
void
pylith::materials::TestMaterial::testUpdateStateVars(void)
{ // testUpdateStateVars
    PYLITH_METHOD_BEGIN;

    CPPUNIT_ASSERT_MESSAGE("Test not implemented.", false); // :TODO: ADD MORE HERE

    PYLITH_METHOD_END;
} // testUpdateStateVars


// ----------------------------------------------------------------------
// Do minimal initilaization of test data.
void
pylith::materials::TestMaterial::_initializeMin(void)
{ // _initializeMin
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);

    pylith::meshio::MeshIOAscii iohandler;
    CPPUNIT_ASSERT(data->meshFilename);
    iohandler.filename(data->meshFilename);
    iohandler.read(_mesh); CPPUNIT_ASSERT(_mesh);

    // Setup coordinates.
    _mesh->coordsys(data->cs);
    CPPUNIT_ASSERT(data->normalizer);
    pylith::topology::MeshOps::nondimensionalize(_mesh, *data->normalizer);

    // id and label initialized in derived class
    material->normalizer(*data->normalizer);

    // Setup solution fields.
    delete _solutionFields; _solutionFields = new pylith::topology::Fields(*_mesh);CPPUNIT_ASSERT(_solutionFields);
    _solutionFields->add("solution","solution");
    _solutionFields->add("solution_dot","solution_dot");
    _solutionFields->add("perturbation","perturbation");
    _solutionFields->add("perturbation_dot","perturbation_dot");
    this->_setupSolutionFields();

    PYLITH_METHOD_END;
} // _initializeMin


// ----------------------------------------------------------------------
// Complete initilaization of test data.
void
pylith::materials::TestMaterial::_initializeFull(void)
{ // _initializeFull
    PYLITH_METHOD_BEGIN;

    Material* material = _material(); CPPUNIT_ASSERT(material);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);
    CPPUNIT_ASSERT(_mesh);

    // Set auxiliary fields spatial database.
    material->auxFieldDB(data->auxDB);

    for (int i = 0; i < data->numAuxSubfields; ++i) {
        const pylith::topology::FieldBase::Discretization& info = data->auxDiscretizations[i];
        material->auxSubfieldDiscretization(data->auxSubfields[i], info.basisOrder, info.quadOrder, info.isBasisContinuous, info.feSpace);
    } // for

    CPPUNIT_ASSERT(_solutionFields);
    material->initialize(_solutionFields->get("solution"));

    PYLITH_METHOD_END;
} // _initializeFull


// ----------------------------------------------------------------------
// Set field to zero on the boundary.
void
pylith::materials::TestMaterial::_zeroBoundary(pylith::topology::Field* field,
                                               PetscMat matrix)
{ // _zeroBoundary
    PYLITH_METHOD_BEGIN;

    CPPUNIT_ASSERT(field);
    TestMaterial_Data* data = _data(); CPPUNIT_ASSERT(data);
    CPPUNIT_ASSERT(data->boundaryLabel);

    PetscDM dmMesh = field->mesh().dmMesh(); CPPUNIT_ASSERT(dmMesh);
    PetscDMLabel label = NULL;
    PetscIS pointIS = NULL;
    const PetscInt *points;
    PetscInt numPoints = 0;
    PetscBool hasLabel = PETSC_FALSE;
    PetscErrorCode err;
    err = DMHasLabel(dmMesh, data->boundaryLabel, &hasLabel); CPPUNIT_ASSERT(!err); CPPUNIT_ASSERT(hasLabel);
    err = DMGetLabel(dmMesh, data->boundaryLabel, &label); CPPUNIT_ASSERT(!err);
    err = DMLabelGetStratumIS(label, 1, &pointIS); CPPUNIT_ASSERT(!err); CPPUNIT_ASSERT(pointIS);
    err = ISGetLocalSize(pointIS, &numPoints); CPPUNIT_ASSERT(!err);
    err = ISGetIndices(pointIS, &points); CPPUNIT_ASSERT(!err);

    pylith::topology::VecVisitorMesh fieldVisitor(*field);
    PylithScalar* fieldArray = fieldVisitor.localArray(); CPPUNIT_ASSERT(fieldArray);

    for (PylithInt p = 0; p < numPoints; ++p) {
        const PylithInt p_bc = points[p];

        const PylithInt off = fieldVisitor.sectionOffset(p_bc);
        const PylithInt dof = fieldVisitor.sectionDof(p_bc);
        for (PylithInt i = 0; i < dof; ++i) {
            fieldArray[off+i] = 0.0;
        } // for
    } // for

    if (matrix) {
        // Use point IS and section offsets to create list of degrees of
        // freedom on boundary in order to zero rows and columns of
        // matrix.
        const int maxDOF = (numPoints > 0) ? fieldVisitor.sectionOffset(points[numPoints-1])+fieldVisitor.sectionDof(points[numPoints-1]) : 0;
        PylithInt* boundaryDOF = (maxDOF > 0) ? new PylithInt[maxDOF] : NULL;
        PylithInt numDOF = 0;

        for (PylithInt p = 0; p < numPoints; ++p) {
            const PylithInt p_bc = points[p];

            const PylithInt off = fieldVisitor.sectionOffset(p_bc);
            const PylithInt dof = fieldVisitor.sectionDof(p_bc);
            for (PylithInt i = 0; i < dof; ++i) {
                boundaryDOF[numDOF++] = off + i;
            } // for
        } // for
        err = MatZeroRowsColumns(matrix, numDOF, boundaryDOF, 0.0, NULL, NULL); CPPUNIT_ASSERT(!err);
        delete[] boundaryDOF; boundaryDOF = NULL;
    } // if

    err = ISRestoreIndices(pointIS, &points); PYLITH_CHECK_ERROR(err);
    err = ISDestroy(&pointIS); PYLITH_CHECK_ERROR(err);

    PYLITH_METHOD_END;
} // _zeroBoundary


// ----------------------------------------------------------------------
// Add small, random perturbations to field.
void
pylith::materials::TestMaterial::_addRandomPerturbation(pylith::topology::Field* field,
                                                        const pylith::topology::Field& fieldRef,
                                                        const PylithReal limit) {
    PYLITH_METHOD_BEGIN;

    CPPUNIT_ASSERT(field);

    PetscErrorCode err;
    PetscRandom random = NULL;
    err = PetscRandomCreate(PETSC_COMM_SELF, &random); CPPUNIT_ASSERT(!err);
    err = PetscRandomSetType(random, PETSCRAND48); CPPUNIT_ASSERT(!err);
    err = PetscRandomSetInterval(random, -limit, +limit); CPPUNIT_ASSERT(!err);
    err = VecSetRandom(field->localVector(), random); CPPUNIT_ASSERT(!err);
    err = PetscRandomDestroy(&random); CPPUNIT_ASSERT(!err);

    _zeroBoundary(field);

    err = VecAXPY(field->localVector(), 1.0, fieldRef.localVector()); CPPUNIT_ASSERT(!err);

    PYLITH_METHOD_END;
} // _addRandomPerturbation


// ----------------------------------------------------------------------
// Constructor
pylith::materials::TestMaterial_Data::TestMaterial_Data(void) :
    dimension(0),
    meshFilename(0),
    boundaryLabel(NULL),
    cs(NULL),

    normalizer(new spatialdata::units::Nondimensional),

    t(0.0),
    dt(0.0),
    tshift(0.0),
    perturbation(1.0e-4),

    numSolnSubfields(0),
    solnDiscretizations(NULL),
    solnDB(new spatialdata::spatialdb::UserFunctionDB),

    numAuxSubfields(0),
    auxSubfields(NULL),
    auxDiscretizations(NULL),
    auxDB(new spatialdata::spatialdb::UserFunctionDB),

    isExplicit(false)
{ // constructor
    CPPUNIT_ASSERT(normalizer);

    CPPUNIT_ASSERT(solnDB);
    solnDB->label("solution");

    CPPUNIT_ASSERT(auxDB);
    auxDB->label("auxiliary field");
} // constructor


// ----------------------------------------------------------------------
// Destructor
pylith::materials::TestMaterial_Data::~TestMaterial_Data(void)
{ // destructor
    delete cs; cs = NULL;
    delete normalizer; normalizer = NULL;
    delete solnDB; solnDB = NULL;
    delete auxDB; auxDB = NULL;
} // destructor


// End of file
