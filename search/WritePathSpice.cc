// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <string>
#include <iostream>
#include <fstream>
#include "Machine.hh"
#include "Debug.hh"
#include "Error.hh"
#include "Report.hh"
#include "StringUtil.hh"
#include "FuncExpr.hh"
#include "Units.hh"
#include "Sequential.hh"
#include "TableModel.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "Parasitics.hh"
#include "PathAnalysisPt.hh"
#include "Path.hh"
#include "PathRef.hh"
#include "PathExpanded.hh"
#include "StaState.hh"
#include "Sim.hh"
#include "WritePathSpice.hh"

namespace sta {

using std::string;
using std::ofstream;
using std::ifstream;

typedef Map<string, StringVector*> CellSpicePortNames;
typedef int Stage;
typedef Map<ParasiticNode*, int> ParasiticNodeMap;
typedef Map<LibertyPort*, LogicValue> LibertyPortLogicValues;

void
streamPrint(ofstream &stream,
	    const char *fmt,
	    ...) __attribute__((format (printf, 2, 3)));

////////////////////////////////////////////////////////////////

class WritePathSpice : public StaState
{
public:
  WritePathSpice(Path *path,
		 const char *spice_filename,
		 const char *subckt_filename,
		 const char *lib_subckt_filename,
		 const char *model_filename,
		 const char *power_name,
		 const char *gnd_name,
		 const StaState *sta);
  ~WritePathSpice();
  void writeSpice();;

private:
  void writeHeader();
  void writeStageInstances();
  void writeInputSource();
  void writeStepVoltSource(const Pin *pin,
			   const TransRiseFall *tr,
			   float slew,
			   float time,
			   int &volt_index);
  void writeStageSubckts();
  void writeInputStage(Stage stage);
  void writeMeasureStmts();
  void writeMeasureStmt(const Pin *pin);
  void writeGateStage(Stage stage);
  void writeSubcktInst(const Pin *input_pin);
  void writeSubcktInstVoltSrcs(Stage stage,
			       const Pin *input_pin,
			       int &volt_index,
			       LibertyPortLogicValues &port_values,
			       const Clock *clk,
			       DcalcAPIndex dcalc_ap_index);
  void writeStageParasitics(Stage stage);
  void writeSubckts();
  void findPathCellnames(// Return values.
			 StringSet &path_cell_names);
  void recordSpicePortNames(const char *cell_name,
			    StringVector &tokens);
  float maxTime();
  const char *nodeName(ParasiticNode *node);
  void initNodeMap(const char *net_name);
  const char *spiceTrans(const TransRiseFall *tr);
  void writeMeasureDelayStmt(Stage stage,
			     Path *from_path,
			     Path *to_path);
  void writeMeasureSlewStmt(Stage stage,
			    Path *path);
  void gatePortValues(Stage stage,
		      // Return values.
		      LibertyPortLogicValues &port_values,
		      const Clock *&clk,
		      DcalcAPIndex &dcalc_ap_index);
  void regPortValues(Stage stage,
		     // Return values.
		     LibertyPortLogicValues &port_values,
		     const Clock *&clk,
		     DcalcAPIndex &dcalc_ap_index);
  void gatePortValues(const Instance *inst,
		      FuncExpr *expr,
		      LibertyPort *input_port,
		      // Return values.
		      LibertyPortLogicValues &port_values);
  void seqPortValues(Sequential *seq,
		     const TransRiseFall *tr,
		     // Return values.
		     LibertyPortLogicValues &port_values);
  void writeInputWaveform();
  void writeClkWaveform();
  void writeWaveformEdge(const TransRiseFall *tr,
			 float time,
			 float slew);
  void writeClkedStepSource(const Pin *pin,
			    const TransRiseFall *tr,
			    const Clock *clk,
			    DcalcAPIndex dcalc_ap_index,
			    int &volt_index);
  float clkWaveformTImeOffset(const Clock *clk);
  float findSlew(Path *path);
  float findSlew(Path *path,
		 const TransRiseFall *tr,
		 TimingArc *next_arc);
  float findSlew(Vertex *vertex,
		 const TransRiseFall *tr,
		 TimingArc *next_arc,
		 DcalcAPIndex dcalc_ap_index);
  LibertyPort *onePort(FuncExpr *expr);
  void writeVoltageSource(const char *inst_name,
			  const char *port_name,
			  float voltage,
			  int &volt_index);
  void writeVoltageSource(LibertyCell *cell,
			  const char *inst_name,
			  const char *subckt_port_name,
			  const char *pg_port_name,
			  float voltage,
			  int &volt_index);
  float slewAxisMinValue(TimingArc *arc);
  float pgPortVoltage(LibertyPgPort *pg_port);

  // Stage "accessors".
  //
  //           stage
  //      |---------------|
  //        |\             |\   .
  // -------| >---/\/\/----| >---
  //  gate  |/ drvr    load|/
  //  input
  //
  // A path from an input port has no GateInputPath (the input port is the drvr).
  // Internally a stage index from stageFirst() to stageLast()
  // is turned into an index into path_expanded_.
  //
  Stage stageFirst();
  Stage stageLast();
  string stageName(Stage stage);
  int stageGateInputPathIndex(Stage stage);
  int stageDrvrPathIndex(Stage stage);
  int stageLoadPathIndex(Stage stage);
  PathRef *stageGateInputPath(Stage stage);
  PathRef *stageDrvrPath(Stage stage);
  PathRef *stageLoadPath(Stage stage);
  TimingArc *stageGateArc(Stage stage);
  TimingArc *stageWireArc(Stage stage);
  Edge *stageGateEdge(Stage stage);
  Edge *stageWireEdge(Stage stage);
  Pin *stageGateInputPin(Stage stage);
  Pin *stageDrvrPin(Stage stage);
  LibertyPort *stageGateInputPort(Stage stage);
  LibertyPort *stageDrvrPort(Stage stage);
  Pin *stageLoadPin(Stage stage);
  const char *stageGateInputPinName(Stage stage);
  const char *stageDrvrPinName(Stage stage);
  const char *stageLoadPinName(Stage stage);
  LibertyCell *stageLibertyCell(Stage stage);
  Instance *stageInstance(Stage stage);

  Path *path_;
  const char *spice_filename_;
  const char *subckt_filename_;
  const char *lib_subckt_filename_;
  const char *model_filename_;
  const char *power_name_;
  const char *gnd_name_;

  ofstream spice_stream_;
  PathExpanded path_expanded_;
  CellSpicePortNames cell_spice_port_names_;
  ParasiticNodeMap node_map_;
  int next_node_index_;
  const char *net_name_;
  float power_voltage_;
  float gnd_voltage_;
  LibertyLibrary *default_library_;
  // Resistance to use to simulate a short circuit between spice nodes.
  float short_ckt_resistance_;
  // Input clock waveform cycles.
  int clk_cycle_count_;
};

////////////////////////////////////////////////////////////////

class SubcktEndsMissing : public StaException
{
public:
  SubcktEndsMissing(const char *cell_name,
		    const char *subckt_filename);;
  const char *what() const throw();

protected:
  string what_;
};

SubcktEndsMissing::SubcktEndsMissing(const char *cell_name,
				     const char *subckt_filename)
{
  what_ = "Error: spice subckt for cell ";
  what_ += cell_name;
  what_ += " missing .ends in ";
  what_ += subckt_filename;
}

const char *
SubcktEndsMissing::what() const throw()
{
  return what_.c_str();
}

////////////////////////////////////////////////////////////////

void
writePathSpice(Path *path,
	       const char *spice_filename,
	       const char *subckt_filename,
	       const char *lib_subckt_filename,
	       const char *model_filename,
	       const char *power_name,
	       const char *gnd_name,
	       StaState *sta)
{
  WritePathSpice writer(path, spice_filename, subckt_filename,
			lib_subckt_filename, model_filename,
			power_name, gnd_name, sta);
  writer.writeSpice();
}

WritePathSpice::WritePathSpice(Path *path,
			       const char *spice_filename,
			       const char *subckt_filename,
			       const char *lib_subckt_filename,
			       const char *model_filename,
			       const char *power_name,
			       const char *gnd_name,
			       const StaState *sta) :
  StaState(sta),
  path_(path),
  spice_filename_(spice_filename),
  subckt_filename_(subckt_filename),
  lib_subckt_filename_(lib_subckt_filename),
  model_filename_(model_filename),
  power_name_(power_name),
  gnd_name_(gnd_name),
  path_expanded_(sta),
  net_name_(nullptr),
  default_library_(network_->defaultLibertyLibrary()),
  short_ckt_resistance_(.0001),
  clk_cycle_count_(3)
{
  bool exists;
  default_library_->supplyVoltage(power_name_, power_voltage_, exists);
  if (!exists) {
    auto dcalc_ap = path_->dcalcAnalysisPt(this);
    auto op_cond = dcalc_ap->operatingConditions();
    if (op_cond == nullptr)
      op_cond = network_->defaultLibertyLibrary()->defaultOperatingConditions();
    power_voltage_ = op_cond->voltage();
  }
  default_library_->supplyVoltage(gnd_name_, gnd_voltage_, exists);
  if (!exists)
    gnd_voltage_ = 0.0;
}

WritePathSpice::~WritePathSpice()
{
  stringDelete(net_name_);
  cell_spice_port_names_.deleteContents();
}

void
WritePathSpice::writeSpice()
{
  spice_stream_.open(spice_filename_);
  if (spice_stream_.is_open()) {
    path_expanded_.expand(path_, true);
    // Find subckt port names as a side-effect of writeSubckts.
    writeSubckts();
    writeHeader();
    writeStageInstances();
    writeMeasureStmts();
    writeInputSource();
    writeStageSubckts();
    streamPrint(spice_stream_, ".end\n");
    spice_stream_.close();
  }
  else
    throw FileNotWritable(spice_filename_);
}

void
WritePathSpice::writeHeader()
{
  auto min_max = path_->minMax(this);
  auto pvt = sdc_->operatingConditions(min_max);
  if (pvt == nullptr)
    pvt = default_library_->defaultOperatingConditions();
  Path *start_path = path_expanded_.startPath();
  streamPrint(spice_stream_, "* Path from %s %s to %s %s\n",
	      network_->pathName(start_path->pin(this)),
	      start_path->transition(this)->asString(),
	      network_->pathName(path_->pin(this)),
	      path_->transition(this)->asString());
  auto temp = pvt->temperature();
  streamPrint(spice_stream_, ".temp %.1f\n", temp);
  streamPrint(spice_stream_, ".include \"%s\"\n", model_filename_);
  streamPrint(spice_stream_, ".include \"%s\"\n", subckt_filename_);

  auto max_time = maxTime();
  auto time_step = max_time / 1e+3;
  streamPrint(spice_stream_, ".tran %.3g %.3g\n\n",
	      time_step, max_time);
}

float
WritePathSpice::maxTime()
{
  auto input_stage = stageFirst();
  auto input_path = stageDrvrPath(input_stage);
  auto tr = input_path->transition(this);
  auto next_arc = stageGateArc(input_stage + 1);
  auto input_slew = findSlew(input_path, tr, next_arc);
  if (input_path->isClock(this)) {
    auto clk = input_path->clock(this);
    auto period = clk->period();
    float first_edge_offset = period / 10;
    auto max_time = period * clk_cycle_count_ + first_edge_offset;
    return max_time;
  }
  else {
    auto end_slew = findSlew(path_);
    auto max_time = delayAsFloat(input_slew
				 + path_->arrival(this)
				 + end_slew * 2) * 1.5;
    return max_time;
  }
}

void
WritePathSpice::writeStageInstances()
{
  streamPrint(spice_stream_, "*****************\n");
  streamPrint(spice_stream_, "* Stage instances\n");
  streamPrint(spice_stream_, "*****************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    auto stage_name = stageName(stage).c_str();
    if (stage == stageFirst())
      streamPrint(spice_stream_, "x%s %s %s %s\n",
		  stage_name,
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_name);
    else 
      streamPrint(spice_stream_, "x%s %s %s %s %s\n",
		  stage_name,
		  stageGateInputPinName(stage),
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_name);
  }
  streamPrint(spice_stream_, "\n");
}

float
WritePathSpice::pgPortVoltage(LibertyPgPort *pg_port)
{
  LibertyLibrary *liberty = pg_port->cell()->libertyLibrary();
  float voltage = 0.0;
  bool exists;
  auto voltage_name = pg_port->voltageName();
  if (voltage_name) {
    liberty->supplyVoltage(voltage_name, voltage, exists);
    if (!exists) {
      if (stringEqual(voltage_name, power_name_))
	voltage = power_voltage_;
      else if (stringEqual(voltage_name, gnd_name_))
	voltage = gnd_voltage_;
      else
	report_->error("pg_pin %s/%s voltage %s not found,\n",
		       pg_port->cell()->name(),
		       pg_port->name(),
		       voltage_name);
    }
  }
  else
    report_->error("Liberty pg_port %s/%s missing voltage_name attribute,\n",
		   pg_port->cell()->name(),
		   pg_port->name());
  return voltage;
}

void
WritePathSpice::writeInputSource()
{
  streamPrint(spice_stream_, "**************\n");
  streamPrint(spice_stream_, "* Input source\n");
  streamPrint(spice_stream_, "**************\n\n");

  auto input_stage = stageFirst();
  auto input_path = stageDrvrPath(input_stage);
  if (input_path->isClock(this))
    writeClkWaveform();
  else
    writeInputWaveform();
  streamPrint(spice_stream_, "\n");
}

void
WritePathSpice::writeInputWaveform()
{
  auto input_stage = stageFirst();
  auto input_path = stageDrvrPath(input_stage);
  auto tr = input_path->transition(this);
  auto next_arc = stageGateArc(input_stage + 1);
  auto slew0 = findSlew(input_path, tr, next_arc);
  // Arbitrary offset.
  auto time0 = slew0;
  int volt_index = 1;
  auto drvr_pin = stageDrvrPin(input_stage);
  writeStepVoltSource(drvr_pin, tr, slew0, time0, volt_index);
}

void
WritePathSpice::writeStepVoltSource(const Pin *pin,
				    const TransRiseFall *tr,
				    float slew,
				    float time,
				    int &volt_index)
{
  float volt0, volt1;
  if (tr == TransRiseFall::rise()) {
    volt0 = gnd_voltage_;
    volt1 = power_voltage_;
  }
  else {
    volt0 = power_voltage_;
    volt1 = gnd_voltage_;
  }
  streamPrint(spice_stream_, "v%d %s 0 pwl(\n",
	      volt_index,
	      network_->pathName(pin));
  streamPrint(spice_stream_, "+%.3e %.3e\n", 0.0, volt0);
  writeWaveformEdge(tr, time, slew);
  streamPrint(spice_stream_, "+%.3e %.3e\n", maxTime(), volt1);
  streamPrint(spice_stream_, "+)\n");
  volt_index++;
}

void
WritePathSpice::writeClkWaveform()
{
  auto input_stage = stageFirst();
  auto input_path = stageDrvrPath(input_stage);
  auto next_arc = stageGateArc(input_stage + 1);
  auto clk_edge = input_path->clkEdge(this);
  auto clk = clk_edge->clock();
  auto period = clk->period();
  float time_offset = clkWaveformTImeOffset(clk);
  TransRiseFall *tr0, *tr1;
  float volt0;
  if (clk_edge->time() < period) {
    tr0 = TransRiseFall::rise();
    tr1 = TransRiseFall::fall();
    volt0 = gnd_voltage_;
  }
  else {
    tr0 = TransRiseFall::fall();
    tr1 = TransRiseFall::rise();
    volt0 = power_voltage_;
  }
  auto slew0 = findSlew(input_path, tr0, next_arc);
  auto slew1 = findSlew(input_path, tr1, next_arc);
  streamPrint(spice_stream_, "v1 %s 0 pwl(\n",
	      stageDrvrPinName(input_stage));
  streamPrint(spice_stream_, "+%.3e %.3e\n", 0.0, volt0);
  for (int cycle = 0; cycle < clk_cycle_count_; cycle++) {
    auto time0 = time_offset + cycle * period;
    auto time1 = time0 + period / 2.0;
    writeWaveformEdge(tr0, time0, slew0);
    writeWaveformEdge(tr1, time1, slew1);
  }
  streamPrint(spice_stream_, "+%.3e %.3e\n", maxTime(), volt0);
  streamPrint(spice_stream_, "+)\n");
}

float
WritePathSpice::clkWaveformTImeOffset(const Clock *clk)
{
  return clk->period() / 10;
}

float
WritePathSpice::findSlew(Path *path)
{
  auto vertex = path->vertex(this);
  auto dcalc_ap_index = path->dcalcAnalysisPt(this)->index();
  auto tr = path->transition(this);
  return findSlew(vertex, tr, nullptr, dcalc_ap_index);
}

float
WritePathSpice::findSlew(Path *path,
			 const TransRiseFall *tr,
			 TimingArc *next_arc)
{
  auto vertex = path->vertex(this);
  auto dcalc_ap_index = path->dcalcAnalysisPt(this)->index();
  return findSlew(vertex, tr, next_arc, dcalc_ap_index);
}

float
WritePathSpice::findSlew(Vertex *vertex,
			 const TransRiseFall *tr,
			 TimingArc *next_arc,
			 DcalcAPIndex dcalc_ap_index)
{
  auto slew = delayAsFloat(graph_->slew(vertex, tr, dcalc_ap_index));
  if (slew == 0.0 && next_arc)
    slew = slewAxisMinValue(next_arc);
  if (slew == 0.0)
    slew = units_->timeUnit()->scale();
  return slew;
}

// Look up the smallest slew axis value in the timing arc delay table.
float
WritePathSpice::slewAxisMinValue(TimingArc *arc)
{
  GateTableModel *gate_model = dynamic_cast<GateTableModel*>(arc->model());
  if (gate_model) {
    const TableModel *model = gate_model->delayModel();
    TableAxis *axis;
    TableAxisVariable var;
    axis = model->axis1();
    var = axis->variable();
    if (var == TableAxisVariable::input_transition_time
	|| var == TableAxisVariable::input_net_transition)
      return axis->axisValue(0);

    axis = model->axis2();
    var = axis->variable();
    if (var == TableAxisVariable::input_transition_time
	|| var == TableAxisVariable::input_net_transition)
      return axis->axisValue(0);

    axis = model->axis3();
    var = axis->variable();
    if (var == TableAxisVariable::input_transition_time
	|| var == TableAxisVariable::input_net_transition)
      return axis->axisValue(0);
  }
  return 0.0;
}

// Write PWL rise/fall edge that crosses threshold at time.
void
WritePathSpice::writeWaveformEdge(const TransRiseFall *tr,
				  float time,
				  float slew)
{
  float volt0, volt1;
  if (tr == TransRiseFall::rise()) {
    volt0 = gnd_voltage_;
    volt1 = power_voltage_;
  }
  else {
    volt0 = power_voltage_;
    volt1 = gnd_voltage_;
  }
  auto threshold = default_library_->inputThreshold(tr);
  auto lower = default_library_->slewLowerThreshold(tr);
  auto upper = default_library_->slewUpperThreshold(tr);
  auto dt = slew / (upper - lower);
  auto time0 = time - dt * threshold;
  auto time1 = time0 + dt;
  streamPrint(spice_stream_, "+%.3e %.3e\n", time0, volt0);
  streamPrint(spice_stream_, "+%.3e %.3e\n", time1, volt1);
}

////////////////////////////////////////////////////////////////

void
WritePathSpice::writeMeasureStmts()
{
  streamPrint(spice_stream_, "********************\n");
  streamPrint(spice_stream_, "* Measure statements\n");
  streamPrint(spice_stream_, "********************\n\n");

  for (auto stage = stageFirst(); stage <= stageLast(); stage++) {
    auto gate_input_path = stageGateInputPath(stage);
    auto drvr_path = stageDrvrPath(stage);
    auto load_path = stageLoadPath(stage);
    if (gate_input_path) {
      // gate input -> gate output
      writeMeasureSlewStmt(stage, gate_input_path);
      writeMeasureDelayStmt(stage, gate_input_path, drvr_path);
    }
    writeMeasureSlewStmt(stage, drvr_path);
    // gate output | input port -> load
    writeMeasureDelayStmt(stage, drvr_path, load_path);
    if (stage == stageLast())
      writeMeasureSlewStmt(stage, load_path);
  }
  streamPrint(spice_stream_, "\n");
}

void
WritePathSpice::writeMeasureDelayStmt(Stage stage,
				      Path *from_path,
				      Path *to_path)
{
  auto from_pin_name = network_->pathName(from_path->pin(this));
  auto from_tr = from_path->transition(this);
  auto from_threshold = power_voltage_ * default_library_->inputThreshold(from_tr);

  auto to_pin_name = network_->pathName(to_path->pin(this));
  auto to_tr = to_path->transition(this);
  auto to_threshold = power_voltage_ * default_library_->inputThreshold(to_tr);
  streamPrint(spice_stream_,
	      ".measure tran %s_%s_delay_%s\n",
	      stageName(stage).c_str(),
	      from_pin_name,
	      to_pin_name);
  streamPrint(spice_stream_,
	      "+trig v(%s) val=%.3f %s=last\n",
	      from_pin_name,
	      from_threshold,
	      spiceTrans(from_tr));
  streamPrint(spice_stream_,
	      "+targ v(%s) val=%.3f %s=last\n",
	      to_pin_name,
	      to_threshold,
	      spiceTrans(to_tr));
}

void
WritePathSpice::writeMeasureSlewStmt(Stage stage,
				     Path *path)
{
  auto pin_name = network_->pathName(path->pin(this));
  auto tr = path->transition(this);
  auto spice_tr = spiceTrans(tr);
  auto lower = power_voltage_ * default_library_->slewLowerThreshold(tr);
  auto upper = power_voltage_ * default_library_->slewUpperThreshold(tr);
  float threshold1, threshold2;
  if (tr == TransRiseFall::rise()) {
    threshold1 = lower;
    threshold2 = upper;
  }
  else {
    threshold1 = upper;
    threshold2 = lower;
  }
  streamPrint(spice_stream_,
	      ".measure tran %s_%s_slew\n",
	      stageName(stage).c_str(),
	      pin_name);
  streamPrint(spice_stream_,
	      "+trig v(%s) val=%.3f %s=last\n",
	      pin_name,
	      threshold1,
	      spice_tr);
  streamPrint(spice_stream_,
	      "+targ v(%s) val=%.3f %s=last\n",
	      pin_name,
	      threshold2,
	      spice_tr);
}

const char *
WritePathSpice::spiceTrans(const TransRiseFall *tr)
{
  if (tr == TransRiseFall::rise())
    return "RISE";
  else
    return "FALL";
}

void
WritePathSpice::writeStageSubckts()
{
  streamPrint(spice_stream_, "***************\n");
  streamPrint(spice_stream_, "* Stage subckts\n");
  streamPrint(spice_stream_, "***************\n\n");

  for (auto stage = stageFirst(); stage <= stageLast(); stage++) {
    if (stage == stageFirst())
      writeInputStage(stage);
    else
      writeGateStage(stage);
  }
}

// Input port to first gate input.
void
WritePathSpice::writeInputStage(Stage stage)
{
  // Input arc.
  // External driver not handled.
  auto drvr_pin_name = stageDrvrPinName(stage);
  auto load_pin_name = stageLoadPinName(stage);
  streamPrint(spice_stream_, ".subckt %s %s %s\n",
	      stageName(stage).c_str(),
	      drvr_pin_name,
	      load_pin_name);
  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

// Gate and load parasitics.
void
WritePathSpice::writeGateStage(Stage stage)
{
  auto input_pin = stageGateInputPin(stage);
  auto input_pin_name = stageGateInputPinName(stage);
  auto drvr_pin = stageDrvrPin(stage);
  auto drvr_pin_name = stageDrvrPinName(stage);
  auto load_pin = stageLoadPin(stage);
  auto load_pin_name = stageLoadPinName(stage);
  streamPrint(spice_stream_, ".subckt stage%d %s %s %s\n",
	      stage,
	      input_pin_name,
	      drvr_pin_name,
	      load_pin_name);
  // Driver subckt call.
  auto inst = stageInstance(stage);
  auto input_port = stageGateInputPort(stage);
  auto drvr_port = stageDrvrPort(stage);
  streamPrint(spice_stream_, "* Gate %s %s -> %s\n",
	      network_->pathName(inst),
	      input_port->name(),
	      drvr_port->name());
  writeSubcktInst(input_pin);
  LibertyPortLogicValues port_values;
  DcalcAPIndex dcalc_ap_index;
  const Clock *clk;
  int volt_index = 1;
  gatePortValues(stage, port_values, clk, dcalc_ap_index);
  writeSubcktInstVoltSrcs(stage, input_pin, volt_index,
			  port_values, clk, dcalc_ap_index);
  streamPrint(spice_stream_, "\n");

  port_values.clear();
  auto pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    auto pin = pin_iter->next();
    if (pin != drvr_pin
	&& pin != load_pin
	&& network_->direction(pin)->isAnyInput()
	&& !network_->isHierarchical(pin)
	&& !network_->isTopLevelPort(pin)) {
      streamPrint(spice_stream_, "* Side load %s\n", network_->pathName(pin));
      writeSubcktInst(pin);
      writeSubcktInstVoltSrcs(stage, pin, volt_index, port_values, nullptr, 0); 
      streamPrint(spice_stream_, "\n");
    }
  }
  delete pin_iter;

  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

void
WritePathSpice::writeSubcktInst(const Pin *input_pin)
{
  auto inst = network_->instance(input_pin);
  auto inst_name = network_->pathName(inst);
  auto cell = network_->libertyCell(inst);
  auto cell_name = cell->name();
  auto spice_port_names = cell_spice_port_names_[cell_name];
  streamPrint(spice_stream_, "x%s", inst_name);
  for (auto subckt_port_name : *spice_port_names) {
    auto subckt_port_cname = subckt_port_name.c_str();
    auto pin = network_->findPin(inst, subckt_port_cname);
    auto pg_port = cell->findPgPort(subckt_port_cname);
    const char *pin_name;
    if (pin) {
      pin_name = network_->pathName(pin);
      streamPrint(spice_stream_, " %s", pin_name);
    }
    else if (pg_port)
      streamPrint(spice_stream_, " %s/%s", inst_name, subckt_port_cname);
    else if (stringEq(subckt_port_cname, power_name_)
	     || stringEq(subckt_port_cname, gnd_name_))
      streamPrint(spice_stream_, " %s/%s", inst_name, subckt_port_cname);
  }
  streamPrint(spice_stream_, " %s\n", cell_name);
}

// Power/ground and input voltage sources.
void
WritePathSpice::writeSubcktInstVoltSrcs(Stage stage,
					const Pin *input_pin,
					int &volt_index,
					LibertyPortLogicValues &port_values,
					const Clock *clk,
					DcalcAPIndex dcalc_ap_index)

{
  auto inst = network_->instance(input_pin);
  auto cell = network_->libertyCell(inst);
  auto cell_name = cell->name();
  auto spice_port_names = cell_spice_port_names_[cell_name];

  auto drvr_pin = stageDrvrPin(stage);
  auto input_port = network_->libertyPort(input_pin);
  auto drvr_port = network_->libertyPort(drvr_pin);
  auto input_port_name = input_port->name();
  auto drvr_port_name = drvr_port->name();
  auto inst_name = network_->pathName(inst);

  debugPrint1(debug_, "write_spice", 2, "subckt %s\n", cell->name());
  for (auto subckt_port_sname : *spice_port_names) {
    auto subckt_port_name = subckt_port_sname.c_str();
    auto pg_port = cell->findPgPort(subckt_port_name);
    debugPrint2(debug_, "write_spice", 2, " port %s%s\n",
		subckt_port_name,
		pg_port ? " pwr/gnd" : "");
    if (pg_port)
      writeVoltageSource(inst_name, subckt_port_name,
			 pgPortVoltage(pg_port), volt_index);
    else if (stringEq(subckt_port_name, power_name_))
      writeVoltageSource(inst_name, subckt_port_name,
			 power_voltage_, volt_index);
    else if (stringEq(subckt_port_name, gnd_name_))
      writeVoltageSource(inst_name, subckt_port_name,
			 gnd_voltage_, volt_index);
    else if (!(stringEq(subckt_port_name, input_port_name)
		 || stringEq(subckt_port_name, drvr_port_name))) {
      // Input voltage to sensitize path from gate input to output.
      auto port = cell->findLibertyPort(subckt_port_name);
      if (port
	  && port->direction()->isAnyInput()) {
	const Pin *pin = network_->findPin(inst, port);
	// Look for tie high/low or propagated constant values.
	LogicValue port_value = sim_->logicValue(pin);
	if (port_value == LogicValue::unknown) {
	  bool has_value;
	  LogicValue value;
	  port_values.findKey(port, value, has_value);
	  if (has_value)
	    port_value = value;
	}
	switch (port_value) {
	case LogicValue::zero:
	case LogicValue::unknown:
	  writeVoltageSource(cell, inst_name, subckt_port_name,
			     port->relatedGroundPin(),
			     gnd_voltage_,
			     volt_index);
	  break;
	case LogicValue::one:
	  writeVoltageSource(cell, inst_name, subckt_port_name,
			     port->relatedPowerPin(),
			     power_voltage_,
			     volt_index);
	  break;
	case LogicValue::rise:
	  writeClkedStepSource(pin, TransRiseFall::rise(), clk,
			       dcalc_ap_index, volt_index);
	  break;
	case LogicValue::fall:
	  writeClkedStepSource(pin, TransRiseFall::fall(), clk,
			       dcalc_ap_index, volt_index);
	  break;
	}
      }
    }
  }
}

// PWL voltage source that rises half way into the first clock cycle.
void
WritePathSpice::writeClkedStepSource(const Pin *pin,
				     const TransRiseFall *tr,
				     const Clock *clk,
				     DcalcAPIndex dcalc_ap_index,
				     int &volt_index)
{
  auto vertex = graph_->pinLoadVertex(pin);
  auto slew = findSlew(vertex, tr, nullptr, dcalc_ap_index);
  auto time = clkWaveformTImeOffset(clk) + clk->period() / 2.0;
  writeStepVoltSource(pin, tr, slew, time, volt_index);
}

void
WritePathSpice::writeVoltageSource(const char *inst_name,
				   const char *port_name,
				   float voltage,
				   int &volt_index)
{
  streamPrint(spice_stream_, "v%d %s/%s 0 %.3f\n",
	      volt_index,
	      inst_name, port_name,
	      voltage);
  volt_index++;
}

void
WritePathSpice::writeVoltageSource(LibertyCell *cell,
				   const char *inst_name,
				   const char *subckt_port_name,
				   const char *pg_port_name,
				   float voltage,
				   int &volt_index)
{
  if (pg_port_name) {
    auto pg_port = cell->findPgPort(pg_port_name);
    if (pg_port)
      voltage = pgPortVoltage(pg_port);
    else
      report_->error("%s pg_port %s not found,\n",
		     cell->name(),
		     pg_port_name);

  }
  writeVoltageSource(inst_name, subckt_port_name, voltage, volt_index);
}

void
WritePathSpice::gatePortValues(Stage stage,
			       // Return values.
			       LibertyPortLogicValues &port_values,
			       const Clock *&clk,
			       DcalcAPIndex &dcalc_ap_index)
{
  clk = nullptr;
  dcalc_ap_index = 0;

  auto gate_edge = stageGateEdge(stage);
  auto drvr_port = stageDrvrPort(stage);
  if (gate_edge->role()->genericRole() == TimingRole::regClkToQ()) 
    regPortValues(stage, port_values, clk, dcalc_ap_index);
  else if (drvr_port->function()) {
    auto input_pin = stageGateInputPin(stage);
    auto input_port = network_->libertyPort(input_pin);
    auto inst = network_->instance(input_pin);
    gatePortValues(inst, drvr_port->function(), input_port, port_values);
  }
}

void
WritePathSpice::regPortValues(Stage stage,
			      // Return values.
			      LibertyPortLogicValues &port_values,
			      const Clock *&clk,
			      DcalcAPIndex &dcalc_ap_index)
{
  auto drvr_port = stageDrvrPort(stage);
  auto drvr_expr = drvr_port->function();
  if (drvr_expr) {
    auto q_port = drvr_expr->port();
    if (q_port) {
      // Drvr (register/latch output) function should be a reference
      // to an internal port like IQ or IQN.
      auto cell = stageLibertyCell(stage);
      auto seq = cell->outputPortSequential(q_port);
      if (seq) {
	auto drvr_path = stageDrvrPath(stage);
	auto drvr_tr = drvr_path->transition(this);
	seqPortValues(seq, drvr_tr, port_values);
	clk = drvr_path->clock(this);
	dcalc_ap_index = drvr_path->dcalcAnalysisPt(this)->index();
      }
      else
	report_->error("no register/latch found for path from %s to %s,\n",
		       stageGateInputPort(stage)->name(),
		       stageDrvrPort(stage)->name());
    }
  }
}

// Find the logic values for expression inputs to enable paths input_port.
void
WritePathSpice::gatePortValues(const Instance *inst,
			       FuncExpr *expr,
			       LibertyPort *input_port,
			       // Return values.
			       LibertyPortLogicValues &port_values)
{
  auto left = expr->left();
  auto right = expr->right();
  switch (expr->op()) {
  case FuncExpr::op_port:
    break;
  case FuncExpr::op_not:
    gatePortValues(inst, left, input_port, port_values);
    break;
  case FuncExpr::op_or:
    if (left->hasPort(input_port)
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = LogicValue::zero;
    else if (left->hasPort(input_port)
	     && right->op() == FuncExpr::op_not
	     && right->left()->op() == FuncExpr::op_port)
	// input_port + !right_port
	port_values[right->left()->port()] = LogicValue::one;
    else if (right->hasPort(input_port)
	     && left->op() == FuncExpr::op_port)
	port_values[left->port()] = LogicValue::zero;
    else if (right->hasPort(input_port)
	     && left->op() == FuncExpr::op_not
	     && left->left()->op() == FuncExpr::op_port)
      // input_port + !left_port
      port_values[left->left()->port()] = LogicValue::one;
    else {
      gatePortValues(inst, left, input_port, port_values);
      gatePortValues(inst, right, input_port, port_values);
    }
    break;
  case FuncExpr::op_and:
    if (left->hasPort(input_port)
	&& right->op() == FuncExpr::op_port)
	port_values[right->port()] = LogicValue::one;
    else if (left->hasPort(input_port)
	     && right->op() == FuncExpr::op_not
	     && right->left()->op() == FuncExpr::op_port)
      // input_port * !right_port
      port_values[right->left()->port()] = LogicValue::zero;
    else if (right->hasPort(input_port)
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = LogicValue::one;
    else if (right->hasPort(input_port)
	     && left->op() == FuncExpr::op_not
	     && left->left()->op() == FuncExpr::op_port)
      // input_port * !left_port
      port_values[left->left()->port()] = LogicValue::zero;
    else {
      gatePortValues(inst, left, input_port, port_values);
      gatePortValues(inst, right, input_port, port_values);
    }
    break;
  case FuncExpr::op_xor:
    // Need to know timing arc sense to get this right.
    if (left->port() == input_port
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = LogicValue::zero;
    else if (right->port() == input_port
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = LogicValue::zero;
    else {
      gatePortValues(inst, left, input_port, port_values);
      gatePortValues(inst, right, input_port, port_values);
    }
    break;
  case FuncExpr::op_one:
  case FuncExpr::op_zero:
    break;
  }
}

void
WritePathSpice::seqPortValues(Sequential *seq,
			      const TransRiseFall *tr,
			      // Return values.
			      LibertyPortLogicValues &port_values)
{
  auto data = seq->data();
  auto port = onePort(data);
  if (port) {
    auto sense = data->portTimingSense(port);
    switch (sense) {
    case TimingSense::positive_unate:
      if (tr == TransRiseFall::rise())
	port_values[port] = LogicValue::rise;
      else
	port_values[port] = LogicValue::fall;
      break;
    case TimingSense::negative_unate:
      if (tr == TransRiseFall::rise())
	port_values[port] = LogicValue::fall;
      else
	port_values[port] = LogicValue::rise;
      break;
    case TimingSense::non_unate:
    case TimingSense::none:
    case TimingSense::unknown:
    default:
      break;
    }
  }
}

// Pick a port, any port...
LibertyPort *
WritePathSpice::onePort(FuncExpr *expr)
{
  auto left = expr->left();
  auto right = expr->right();
  LibertyPort *port;
  switch (expr->op()) {
  case FuncExpr::op_port:
    return expr->port();
  case FuncExpr::op_not:
    return onePort(left);
  case FuncExpr::op_or:
  case FuncExpr::op_and:
  case FuncExpr::op_xor:
    port = onePort(left);
    if (port == nullptr)
      port = onePort(right);
    return port;
  case FuncExpr::op_one:
  case FuncExpr::op_zero:
  default:
    return nullptr;
  }
}

// Sort predicate for ParasiticDevices.
class ParasiticDeviceLess
{
public:
  ParasiticDeviceLess(Parasitics *parasitics) :
    parasitics_(parasitics)
  {
  }
  bool operator()(const ParasiticDevice *device1,
		  const ParasiticDevice *device2) const
  {
    auto node1 = parasitics_->node1(device1);
    auto node2 = parasitics_->node1(device2);
    auto name1 = parasitics_->name(node1);
    auto name2 = parasitics_->name(node2);
    if (stringEq(name1, name2)) {
      auto node12 = parasitics_->node2(device1);
      auto node22 = parasitics_->node2(device2);
      auto name12 = parasitics_->name(node12);
      auto name22 = parasitics_->name(node22);
      return stringLess(name12, name22);
    }
    else 
      return stringLess(name1, name2);
  }
private:
  Parasitics *parasitics_;
};

// Sort predicate for ParasiticDevices.
class ParasiticNodeLess
{
public:
  ParasiticNodeLess(Parasitics *parasitics) :
    parasitics_(parasitics)
  {
  }
  bool operator()(const ParasiticNode *node1,
		  const ParasiticNode *node2) const
  {
    auto name1 = parasitics_->name(node1);
    auto name2 = parasitics_->name(node2);
    return stringLess(name1, name2);
  }
private:
  Parasitics *parasitics_;
};

void
WritePathSpice::writeStageParasitics(Stage stage)
{
  auto drvr_path = stageDrvrPath(stage);
  auto drvr_pin = stageDrvrPin(stage);
  auto dcalc_ap = drvr_path->dcalcAnalysisPt(this);
  auto parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  auto parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
  Set<const Pin*> reachable_pins;
  int res_index = 1;
  int cap_index = 1;
  if (parasitic) {
    auto net = network_->net(drvr_pin);
    auto net_name = net ? network_->pathName(net) : network_->pathName(drvr_pin);
    initNodeMap(net_name);
    streamPrint(spice_stream_, "* Net %s\n", net_name);

    // Sort devices for consistent regression results.
    Vector<ParasiticDevice*> devices;
    ParasiticDeviceIterator *device_iter1 = parasitics_->deviceIterator(parasitic);
    while (device_iter1->hasNext()) {
      auto device = device_iter1->next();
      devices.push_back(device);
    }
    delete device_iter1;

    sort(devices, ParasiticDeviceLess(parasitics_));

    for (auto device : devices) {
      auto resistance = parasitics_->value(device, parasitic_ap);
      if (parasitics_->isResistor(device)) {
	auto node1 = parasitics_->node1(device);
	auto node2 = parasitics_->node2(device);
	streamPrint(spice_stream_, "R%d %s %s %.3e\n",
		    res_index,
		    nodeName(node1),
		    nodeName(node2),
		    resistance);
	res_index++;

	auto pin1 = parasitics_->connectionPin(node1);
	reachable_pins.insert(pin1);
	auto pin2 = parasitics_->connectionPin(node2);
	reachable_pins.insert(pin2);
      }
      else if (parasitics_->isCouplingCap(device)) {
	// Ground coupling caps for now.
	ParasiticNode *node1 = 	parasitics_->node1(device);
	auto cap = parasitics_->value(device, parasitic_ap);
	streamPrint(spice_stream_, "C%d %s 0 %.3e\n",
		    cap_index,
		    nodeName(node1),
		    cap);
	cap_index++;
      }
    }
  }
  else
    streamPrint(spice_stream_, "* No parasitics found for this net.\n");

  // Add resistors from drvr to load for missing parasitic connections.
  auto pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    auto pin = pin_iter->next();
    if (pin != drvr_pin
	&& network_->isLoad(pin)
	&& !network_->isHierarchical(pin)
	&& !reachable_pins.hasKey(pin)) {
      streamPrint(spice_stream_, "R%d %s %s %.3e\n",
		  res_index,
		  network_->pathName(drvr_pin),
		  network_->pathName(pin),
		  short_ckt_resistance_);
      res_index++;
    }
  }
  delete pin_iter;

  if (parasitic) {
    // Sort node capacitors for consistent regression results.
    Vector<ParasiticNode*> nodes;
    ParasiticNodeIterator *node_iter = parasitics_->nodeIterator(parasitic);
    while (node_iter->hasNext()) {
      auto node = node_iter->next();
      nodes.push_back(node);
    }

    sort(nodes, ParasiticNodeLess(parasitics_));

    for (auto node : nodes) {
      auto cap = parasitics_->nodeGndCap(node, parasitic_ap);
      // Spice has a cow over zero value caps.
      if (cap > 0.0) {
	streamPrint(spice_stream_, "C%d %s 0 %.3e\n",
		    cap_index,
		    nodeName(node),
		    cap);
	cap_index++;
      }
    }
    delete node_iter;
  }
}

void
WritePathSpice::initNodeMap(const char *net_name)
{
  stringDelete(net_name_);
  node_map_.clear();
  next_node_index_ = 1;
  net_name_ = stringCopy(net_name);
}

const char *
WritePathSpice::nodeName(ParasiticNode *node)
{
  auto pin = parasitics_->connectionPin(node);
  if (pin)
    return parasitics_->name(node);
  else {
    int node_index;
    bool node_index_exists;
    node_map_.findKey(node, node_index, node_index_exists);
    if (!node_index_exists) {
      node_index = next_node_index_++;
      node_map_[node] = node_index;
    }
    return stringPrintTmp("%s/%d", net_name_, node_index);
  }
}

////////////////////////////////////////////////////////////////

// Copy the subckt definition from lib_subckt_filename for
// each cell in path to path_subckt_filename.
void
WritePathSpice::writeSubckts()
{
  StringSet path_cell_names;
  findPathCellnames(path_cell_names);

  ifstream lib_subckts_stream(lib_subckt_filename_);
  if (lib_subckts_stream.is_open()) {
    ofstream subckts_stream(subckt_filename_);
    if (subckts_stream.is_open()) {
      string line;
      while (getline(lib_subckts_stream, line)) {
	// .subckt <cell_name> [args..]
	StringVector tokens;
	split(line, " \t", tokens);
	if (tokens.size() >= 2
	    && stringEqual(tokens[0].c_str(), ".subckt")) {
	  auto cell_name = tokens[1].c_str();
	  if (path_cell_names.hasKey(cell_name)) {
	    subckts_stream << line << "\n";
	    bool found_ends = false;
	    while (getline(lib_subckts_stream, line)) {
	      subckts_stream << line << "\n";
	      if (stringBeginEqual(line.c_str(), ".ends")) {
		subckts_stream << "\n";
		found_ends = true;
		break;
	      }
	    }
	    if (!found_ends)
	      throw SubcktEndsMissing(cell_name, lib_subckt_filename_);
	    path_cell_names.erase(cell_name);
	  }
	  recordSpicePortNames(cell_name, tokens);
	}
      }
      subckts_stream.close();
      lib_subckts_stream.close();

      if (!path_cell_names.empty()) {
	report_->error("The following subkcts are missing from %s\n",
		       lib_subckt_filename_);
	for (auto cell_name : path_cell_names)
	  report_->printError(" %s\n", cell_name);
      }
    }
    else {
      lib_subckts_stream.close();
      throw FileNotWritable(subckt_filename_);
    }
  }
  else
    throw FileNotReadable(lib_subckt_filename_);
}

void
WritePathSpice::findPathCellnames(// Return values.
				  StringSet &path_cell_names)
{
  for (auto stage = stageFirst(); stage <= stageLast(); stage++) {
    auto arc = stageGateArc(stage);
    if (arc) {
      auto cell = arc->set()->libertyCell();
      if (cell) {
	debugPrint1(debug_, "write_spice", 2, "cell %s\n", cell->name());
	path_cell_names.insert(cell->name());
      }
      // Include side receivers.
      auto drvr_pin = stageDrvrPin(stage);
      auto pin_iter = network_->connectedPinIterator(drvr_pin);
      while (pin_iter->hasNext()) {
	auto pin = pin_iter->next();
	auto port = network_->libertyPort(pin);
	if (port) {
	  auto cell = port->libertyCell();
	  path_cell_names.insert(cell->name());
	}
      }
      delete pin_iter;
    }
  }
}

void
WritePathSpice::recordSpicePortNames(const char *cell_name,
				     StringVector &tokens)
{
  auto cell = network_->findLibertyCell(cell_name);
  if (cell) {
    auto spice_port_names = new StringVector;
    for (size_t i = 2; i < tokens.size(); i++) {
      auto port_name = tokens[i].c_str();
      auto port = cell->findLibertyPort(port_name);
      auto pg_port = cell->findPgPort(port_name);
      if (port == nullptr
	  && pg_port == nullptr
	  && !stringEqual(port_name, power_name_)
	  && !stringEqual(port_name, gnd_name_))
	report_->error("subckt %s port %s has no corresponding liberty port, pg_port and is not power or ground.\n",
		       cell_name, port_name);
      spice_port_names->push_back(port_name);
    }
    cell_spice_port_names_[cell_name] = spice_port_names;
  }
}

////////////////////////////////////////////////////////////////

Stage
WritePathSpice::stageFirst()
{
  return 1;
}

Stage
WritePathSpice::stageLast()
{
  return (path_expanded_.size() + 1) / 2;
}

string
WritePathSpice::stageName(Stage stage)
{
  string name;
  stringPrint(name, "stage%d", stage);
  return name;
}

int
WritePathSpice::stageGateInputPathIndex(Stage stage)
{
  return stage * 2 - 3;
}

int
WritePathSpice::stageDrvrPathIndex(Stage stage)
{
  return stage * 2 - 2;
}

int
WritePathSpice::stageLoadPathIndex(Stage stage)
{
  return stage * 2 - 1;
}

PathRef *
WritePathSpice::stageGateInputPath(Stage stage)
{
  auto path_index = stageGateInputPathIndex(stage);
  return path_expanded_.path(path_index);
}

PathRef *
WritePathSpice::stageDrvrPath(Stage stage)
{
  auto path_index = stageDrvrPathIndex(stage);
  return path_expanded_.path(path_index);
}

PathRef *
WritePathSpice::stageLoadPath(Stage stage)
{
  auto path_index = stageLoadPathIndex(stage);
  return path_expanded_.path(path_index);
}

TimingArc *
WritePathSpice::stageGateArc(Stage stage)
{
  auto path_index = stageDrvrPathIndex(stage);
  if (path_index >= 0)
    return path_expanded_.prevArc(path_index);
  else
    return nullptr;
}

TimingArc *
WritePathSpice::stageWireArc(Stage stage)
{
  auto path_index = stageLoadPathIndex(stage);
  return path_expanded_.prevArc(path_index);
}

Edge *
WritePathSpice::stageGateEdge(Stage stage)
{
  auto path = stageDrvrPath(stage);
  auto arc = stageGateArc(stage);
  return path->prevEdge(arc, this);
}

Edge *
WritePathSpice::stageWireEdge(Stage stage)
{
  auto path = stageLoadPath(stage);
  auto arc = stageWireArc(stage);
  return path->prevEdge(arc, this);
}

Pin *
WritePathSpice::stageGateInputPin(Stage stage)
{
  auto path = stageGateInputPath(stage);
  return path->pin(this);
}

LibertyPort *
WritePathSpice::stageGateInputPort(Stage stage)
{
  auto pin = stageGateInputPin(stage);
  return network_->libertyPort(pin);
}

Pin *
WritePathSpice::stageDrvrPin(Stage stage)
{
  auto path = stageDrvrPath(stage);
  return path->pin(this);
}

LibertyPort *
WritePathSpice::stageDrvrPort(Stage stage)
{
  auto pin = stageDrvrPin(stage);
  return network_->libertyPort(pin);
}

Pin *
WritePathSpice::stageLoadPin(Stage stage)
{
  auto path = stageLoadPath(stage);
  return path->pin(this);
}

const char *
WritePathSpice::stageGateInputPinName(Stage stage)
{
  auto pin = stageGateInputPin(stage);
  return network_->pathName(pin);
}

const char *
WritePathSpice::stageDrvrPinName(Stage stage)
{
  auto pin = stageDrvrPin(stage);
  return network_->pathName(pin);
}

const char *
WritePathSpice::stageLoadPinName(Stage stage)
{
  auto pin = stageLoadPin(stage);
  return network_->pathName(pin);
}

Instance *
WritePathSpice::stageInstance(Stage stage)
{
  auto pin = stageDrvrPin(stage);
  return network_->instance(pin);
}

LibertyCell *
WritePathSpice::stageLibertyCell(Stage stage)
{
  auto pin = stageDrvrPin(stage);
  return network_->libertyPort(pin)->libertyCell();
}

////////////////////////////////////////////////////////////////

// fprintf for c++ streams.
// Yes, I hate formatted output to ostream THAT much.
void
streamPrint(ofstream &stream,
	    const char *fmt,
	    ...)
{
  va_list args;
  va_start(args, fmt);
  char *result;
  if (vasprintf(&result, fmt, args) == -1)
    internalError("out of memory");
  stream << result;
  free(result);
  va_end(args);
}

} // namespace
