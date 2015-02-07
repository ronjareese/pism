// Copyright (C) 2011, 2012, 2013, 2014, 2015 PISM Authors
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

#include "PA_frac_P.hh"
#include "PISMConfig.hh"

namespace pism {
namespace atmosphere {

Frac_P::Frac_P(const IceGrid &g, AtmosphereModel* in)
  : PScalarForcing<AtmosphereModel,PAModifier>(g, in),
    air_temp(g.config.get_unit_system(), "air_temp", m_grid),
    precipitation(g.config.get_unit_system(), "precipitation", m_grid)
{
  offset = NULL;

  option_prefix = "-atmosphere_frac_P";
  offset_name = "frac_P";
  offset = new Timeseries(&m_grid, offset_name, m_config.get_string("time_dimension_name"));
  offset->metadata().set_units("1");
  offset->metadata().set_string("long_name", "precipitation multiplier, pure fraction");
  offset->dimension_metadata().set_units(m_grid.time->units_string());

  air_temp.set_string("pism_intent", "diagnostic");
  air_temp.set_string("long_name", "near-surface air temperature");
  air_temp.set_units("K");

  precipitation.set_string("pism_intent", "diagnostic");
  precipitation.set_string("long_name", "precipitation, units of ice-equivalent thickness per time");
  precipitation.set_units("m / s");
  precipitation.set_glaciological_units("m / year");
}

Frac_P::~Frac_P()
{
  // empty; "offset" is deleted by ~PScalarForcing().
}

void Frac_P::init() {

  m_t = m_dt = GSL_NAN;  // every re-init restarts the clock

  input_model->init();

  verbPrintf(2, m_grid.com,
             "* Initializing precipitation forcing using scalar multipliers...\n");

  init_internal();
}

MaxTimestep Frac_P::max_timestep_impl(double t) {
  (void) t;
  return MaxTimestep();
}

void Frac_P::init_timeseries(const std::vector<double> &ts) {

  PAModifier::init_timeseries(ts);

  m_offset_values.resize(m_ts_times.size());
  for (unsigned int k = 0; k < m_ts_times.size(); ++k) {
    m_offset_values[k] = (*offset)(m_ts_times[k]);
  }
}

void Frac_P::mean_precipitation(IceModelVec2S &result) {
  input_model->mean_precipitation(result);
  scale_data(result);
}

void Frac_P::precip_time_series(int i, int j, std::vector<double> &result) {
  input_model->precip_time_series(i, j, result);

  for (unsigned int k = 0; k < m_ts_times.size(); ++k) {
    result[k] *= m_offset_values[k];
  }
}

void Frac_P::add_vars_to_output_impl(const std::string &keyword,
                                   std::set<std::string> &result) {
  input_model->add_vars_to_output(keyword, result);

  if (keyword == "medium" || keyword == "big") {
    result.insert("air_temp");
    result.insert("precipitation");
  }
}

void Frac_P::define_variables_impl(const std::set<std::string> &vars_input,
                                           const PIO &nc, IO_Type nctype) {
  std::set<std::string> vars = vars_input;

  if (set_contains(vars, "air_temp")) {
    air_temp.define(nc, nctype, false);
    vars.erase("air_temp");
  }

  if (set_contains(vars, "precipitation")) {
    precipitation.define(nc, nctype, true);
    vars.erase("precipitation");
  }

  input_model->define_variables(vars, nc, nctype);
}

void Frac_P::write_variables_impl(const std::set<std::string> &vars_input,
                                          const PIO &nc) {
  std::set<std::string> vars = vars_input;

  if (set_contains(vars, "air_temp")) {
    IceModelVec2S tmp;
    tmp.create(m_grid, "air_temp", WITHOUT_GHOSTS);
    tmp.metadata() = air_temp;

    mean_annual_temp(tmp);

    tmp.write(nc);

    vars.erase("air_temp");
  }

  if (set_contains(vars, "precipitation")) {
    IceModelVec2S tmp;
    tmp.create(m_grid, "precipitation", WITHOUT_GHOSTS);
    tmp.metadata() = precipitation;

    mean_precipitation(tmp);

    tmp.write_in_glaciological_units = true;
    tmp.write(nc);

    vars.erase("precipitation");
  }

  input_model->write_variables(vars, nc);
}

} // end of namespace atmosphere
} // end of namespace pism