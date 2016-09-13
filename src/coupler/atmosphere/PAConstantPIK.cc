// Copyright (C) 2011, 2012, 2013, 2014, 2015, 2016 PISM Authors
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <gsl/gsl_math.h>

#include "PAConstantPIK.hh"
#include "base/util/pism_utilities.hh"
#include "base/util/PISMVars.hh"
#include "base/util/IceGrid.hh"
#include "base/util/PISMConfigInterface.hh"
#include "base/util/pism_const.hh"
#include "base/util/io/io_helpers.hh"
#include "base/util/MaxTimestep.hh"

namespace pism {
namespace atmosphere {

PIK::PIK(IceGrid::ConstPtr g)
  : AtmosphereModel(g) {

  m_precipitation_vec.create(m_grid, "precipitation", WITHOUT_GHOSTS);
  m_precipitation_vec.set_attrs("model_state", "precipitation rate",
                                "kg m-2 second-1", "", 0);
  m_precipitation_vec.metadata(0).set_string("glaciological_units", "kg m-2 year-1");
  m_precipitation_vec.write_in_glaciological_units = true;
  m_precipitation_vec.set_time_independent(true);

  m_air_temp_vec.create(m_grid, "air_temp", WITHOUT_GHOSTS);
  m_air_temp_vec.set_attrs("model_state", "mean annual near-surface air temperature",
                           "Kelvin", "", 0);
  m_air_temp_vec.set_time_independent(true);
}

void PIK::mean_precipitation_impl(IceModelVec2S &result) {
  result.copy_from(m_precipitation_vec);
}

void PIK::mean_annual_temp_impl(IceModelVec2S &result) {
  result.copy_from(m_air_temp_vec);
}

void PIK::begin_pointwise_access_impl() {
  m_precipitation_vec.begin_access();
  m_air_temp_vec.begin_access();
}

void PIK::end_pointwise_access_impl() {
  m_precipitation_vec.end_access();
  m_air_temp_vec.end_access();
}

void PIK::temp_time_series_impl(int i, int j, std::vector<double> &result) {
  for (unsigned int k = 0; k < m_ts_times.size(); k++) {
    result[k] = m_air_temp_vec(i,j);
  }
}

void PIK::precip_time_series_impl(int i, int j, std::vector<double> &result) {
  for (unsigned int k = 0; k < m_ts_times.size(); k++) {
    result[k] = m_precipitation_vec(i,j);
  }
}

void PIK::add_vars_to_output_impl(const std::string &keyword, std::set<std::string> &result) {
  (void) keyword;
  result.insert("precipitation");
}

void PIK::define_variables_impl(const std::set<std::string> &vars, const PIO &nc,
                                            IO_Type nctype) {

  if (set_contains(vars, "precipitation")) {
    m_precipitation_vec.define(nc, nctype);
  }
}

void PIK::write_variables_impl(const std::set<std::string> &vars, const PIO &nc) {

  if (set_contains(vars, "precipitation")) {
    m_precipitation_vec.write(nc);
  }
}

void PIK::init_impl() {
  m_t = m_dt = GSL_NAN;  // every re-init restarts the clock

  m_log->message(2,
             "* Initializing the constant-in-time atmosphere model PIK.\n"
             "  It reads a precipitation field directly from the file and holds it constant.\n"
             "  Near-surface air temperature is parameterized as in Martin et al. 2011, Eqn. 2.0.2.\n");

  InputOptions opts = process_input_options(m_grid->com);

  // read snow precipitation rate and air_temps from file
  m_log->message(2,
             "    reading mean annual ice-equivalent precipitation rate 'precipitation'\n"
             "    from %s ... \n",
             opts.filename.c_str());
  if (opts.type == INIT_BOOTSTRAP) {
    m_precipitation_vec.regrid(opts.filename, CRITICAL);
  } else {
    m_precipitation_vec.read(opts.filename, opts.record); // fails if not found!
  }
}

MaxTimestep PIK::max_timestep_impl(double t) {
  (void) t;
  return MaxTimestep();
}

void PIK::update_impl(double, double) {
  // Compute near-surface air temperature using a latitude- and
  // elevation-dependent parameterization:

  const IceModelVec2S
    &elevation = *m_grid->variables().get_2d_scalar("surface_altitude"),
    &latitude  = *m_grid->variables().get_2d_scalar("latitude");

  IceModelVec::AccessList list;
  list.add(m_air_temp_vec);
  list.add(elevation);
  list.add(latitude);
  for (Points p(*m_grid); p; p.next()) {
    const int i = p.i(), j = p.j();

    m_air_temp_vec(i, j) = 273.15 + 30 - 0.0075 * elevation(i, j) - 0.68775 * latitude(i, j)*(-1.0) ;
  }
}

void PIK::init_timeseries_impl(const std::vector<double> &ts) {
  m_ts_times = ts;
}


} // end of namespace atmosphere
} // end of namespace pism
