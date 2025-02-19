// ---------------------------------------------------------------------
//
// Copyright (C) 2004 - 2020 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


// test the PETSc CG solver with PETSc MatrixFree class


#include <deal.II/lac/petsc_precondition.h>
#include <deal.II/lac/petsc_solver.h>
#include <deal.II/lac/petsc_sparse_matrix.h>
#include <deal.II/lac/petsc_vector.h>
#include <deal.II/lac/vector_memory.h>

#include <iostream>
#include <typeinfo>

#include "../tests.h"

#include "petsc_mf_testmatrix.h"


int
main(int argc, char **argv)
{
  initlog();
  deallog << std::setprecision(4);

  Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);
  {
    SolverControl control(100, 1.e-3);

    const unsigned int size = 32;
    unsigned int       dim  = (size - 1) * (size - 1);

    deallog << "Size " << size << " Unknowns " << dim << std::endl;

    PetscFDMatrix A(size, dim);

    IndexSet indices(dim);
    indices.add_range(0, dim);
    PETScWrappers::MPI::Vector f(indices, MPI_COMM_WORLD);
    PETScWrappers::MPI::Vector u(indices, MPI_COMM_WORLD);
    f = 1.;
    A.compress(VectorOperation::insert);

    PETScWrappers::SolverCG         solver(control);
    PETScWrappers::PreconditionNone preconditioner(A);
    deallog << "Solver type: " << typeid(solver).name() << std::endl;
    check_solver_within_range(solver.solve(A, u, f, preconditioner),
                              control.last_step(),
                              42,
                              44);

    u = 0.;
    PETScWrappers::PreconditionShell preconditioner_user(A);

    // Identity preconditioner
    preconditioner_user.vmult =
      [](PETScWrappers::VectorBase &      dst,
         const PETScWrappers::VectorBase &src) -> int {
      dst = src;
      return 0;
    };

    check_solver_within_range(solver.solve(A, u, f, preconditioner_user),
                              control.last_step(),
                              42,
                              44);
  }
}
