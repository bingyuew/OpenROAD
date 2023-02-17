/////////////////////////////////////////////////////////////////////////////
//
// BSD 3-Clause License
//
// Copyright (c) 2023, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////////

#include "pad/ICeWall.h"

#include <boost/polygon/polygon.hpp>

#include "RDLRouter.h"
#include "Utilities.h"
#include "odb/db.h"
#include "odb/dbTransform.h"
#include "odb/geom.h"
#include "utl/Logger.h"

namespace pad {

ICeWall::ICeWall() : db_(nullptr), logger_(nullptr), router_(nullptr)
{
}

ICeWall::~ICeWall()
{
}

void ICeWall::init(odb::dbDatabase* db, utl::Logger* logger)
{
  db_ = db;
  logger_ = logger;
}

odb::dbBlock* ICeWall::getBlock() const
{
  auto* chip = db_->getChip();
  if (chip == nullptr) {
    return nullptr;
  }

  return chip->getBlock();
}

void ICeWall::assertMasterType(odb::dbMaster* master,
                               odb::dbMasterType type) const
{
  if (master == nullptr) {
    logger_->error(utl::PAD, 23, "Master must be specified.");
  }
  if (master->getType() != type) {
    logger_->error(utl::PAD,
                   11,
                   "{} is not of type {}, but is instead {}",
                   master->getName(),
                   type.getString(),
                   master->getType().getString());
  }
}

void ICeWall::assertMasterType(odb::dbInst* inst, odb::dbMasterType type) const
{
  auto* master = inst->getMaster();
  if (master->getType() != type) {
    logger_->error(utl::PAD,
                   12,
                   "{} is not of type {}, but is instead {}",
                   inst->getName(),
                   type.getString(),
                   master->getType().getString());
  }
}

void ICeWall::makeBumpArray(odb::dbMaster* master,
                            const odb::Point& start,
                            int rows,
                            int columns,
                            int xpitch,
                            int ypitch,
                            const std::string& prefix)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  assertMasterType(master, odb::dbMasterType::COVER_BUMP);

  for (int xn = 0; xn < columns; xn++) {
    for (int yn = 0; yn < rows; yn++) {
      const odb::Point pos(start.x() + xn * xpitch, start.y() + yn * ypitch);
      const std::string name = fmt::format("{}{}_{}", prefix, xn, yn);
      auto* inst = odb::dbInst::create(block, master, name.c_str());

      inst->setOrigin(pos.x(), pos.y());
      inst->setPlacementStatus(odb::dbPlacementStatus::FIRM);
    }
  }
}

void ICeWall::removeBump(odb::dbInst* inst)
{
  if (inst == nullptr) {
    return;
  }

  assertMasterType(inst, odb::dbMasterType::COVER_BUMP);

  odb::dbInst::destroy(inst);
}

void ICeWall::removeBumpArray(odb::dbMaster* master)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  assertMasterType(master, odb::dbMasterType::COVER_BUMP);

  for (auto* inst : block->getInsts()) {
    if (inst->getMaster() == master) {
      removeBump(inst);
    }
  }
}

void ICeWall::makeBTerm(odb::dbNet* net,
                        odb::dbTechLayer* layer,
                        const odb::Rect& shape) const
{
  odb::dbBTerm* bterm = odb::dbBTerm::create(net, net->getConstName());
  if (bterm == nullptr) {
    bterm = net->get1stBTerm();
  }
  bterm->setSigType(net->getSigType());
  odb::dbBPin* pin = odb::dbBPin::create(bterm);
  odb::dbBox::create(
      pin, layer, shape.xMin(), shape.yMin(), shape.xMax(), shape.yMax());
  pin->setPlacementStatus(odb::dbPlacementStatus::FIRM);
}

void ICeWall::assignBump(odb::dbInst* inst, odb::dbNet* net)
{
  if (inst == nullptr) {
    logger_->error(
        utl::PAD, 24, "Instance must be specified to assign it to a bump.");
  }

  if (net == nullptr) {
    logger_->error(
        utl::PAD, 25, "Net must be specified to assign it to a bump.");
  }

  assertMasterType(inst, odb::dbMasterType::COVER_BUMP);

  odb::dbTransform xform;
  inst->getTransform(xform);

  // Connect to all iterms since this is a bump
  for (auto* pin : inst->getITerms()) {
    if (pin->getNet() != net) {
      pin->connect(net);
    }

    for (auto* mpin : pin->getMTerm()->getMPins()) {
      for (auto* geom : mpin->getGeometry()) {
        auto* layer = geom->getTechLayer();
        if (layer == nullptr) {
          continue;
        }

        odb::Rect shape = geom->getBox();
        xform.apply(shape);
        makeBTerm(net, layer, shape);
      }
    }
  }
}

void ICeWall::makeFakeSite(const std::string& name, int width, int height)
{
  const std::string lib_name = "FAKE_IO";
  auto* lib = db_->findLib(lib_name.c_str());
  if (lib == nullptr) {
    lib = odb::dbLib::create(db_, lib_name.c_str());
  }

  auto* site = odb::dbSite::create(lib, name.c_str());
  site->setWidth(width);
  site->setHeight(height);
  site->setClass(odb::dbSiteClass::PAD);
}

void ICeWall::makeIORow(odb::dbSite* horizontal_site,
                        odb::dbSite* vertical_site,
                        odb::dbSite* corner_site,
                        int west_offset,
                        int north_offset,
                        int east_offset,
                        int south_offset,
                        odb::dbOrientType rotation,
                        int ring_index)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  if (horizontal_site == nullptr) {
    logger_->error(utl::PAD, 14, "Horizontal site must be speficied.");
  }
  if (vertical_site == nullptr) {
    logger_->error(utl::PAD, 15, "Vertical site must be speficied.");
  }
  if (corner_site == nullptr) {
    logger_->error(utl::PAD, 16, "Corner site must be speficied.");
  }

  std::string name_format;
  if (ring_index >= 0) {
    name_format = fmt::format("IO_{{}}_{}", ring_index);
  } else {
    name_format = "IO_{}";
  }

  const odb::Rect die = block->getDieArea();

  odb::Rect outer_io(die.xMin() + west_offset,
                     die.yMin() + south_offset,
                     die.xMax() - east_offset,
                     die.yMax() - north_offset);

  const int cheight = corner_site->getHeight();
  const int cwidth
      = std::max(vertical_site->getHeight(), corner_site->getWidth());

  const int x_sites = std::floor(static_cast<double>(outer_io.dx() - 2 * cwidth)
                                 / vertical_site->getWidth());
  outer_io.set_xhi(outer_io.xMin() + 2 * cwidth
                   + x_sites * vertical_site->getWidth());
  const int y_sites
      = std::floor(static_cast<double>(outer_io.dy() - 2 * cheight)
                   / horizontal_site->getWidth());
  outer_io.set_yhi(outer_io.yMin() + 2 * cheight
                   + y_sites * horizontal_site->getWidth());

  const odb::Rect corner_origins(outer_io.xMin(),
                                 outer_io.yMin(),
                                 outer_io.xMax() - cwidth,
                                 outer_io.yMax() - cheight);

  const odb::dbTransform xform(rotation);

  // Create corners
  const int corner_sites
      = std::max(horizontal_site->getHeight(), corner_site->getWidth())
        / corner_site->getWidth();
  auto create_corner = [block, corner_site, corner_sites, &name_format, &xform](
                           const std::string& name,
                           const odb::Point& origin,
                           odb::dbOrientType orient) -> odb::dbRow* {
    const std::string row_name = fmt::format(name_format, name);
    odb::dbTransform rotation(orient);
    rotation.concat(xform);
    return odb::dbRow::create(block,
                              row_name.c_str(),
                              corner_site,
                              origin.x(),
                              origin.y(),
                              rotation.getOrient(),
                              odb::dbRowDir::HORIZONTAL,
                              corner_sites,
                              corner_site->getWidth());
  };
  auto* nw = create_corner(
      "CORNER_NORTH_WEST", corner_origins.ul(), odb::dbOrientType::MX);
  create_corner(
      "CORNER_NORTH_EAST", corner_origins.ur(), odb::dbOrientType::R180);
  auto* se = create_corner(
      "CORNER_SOUTH_EAST", corner_origins.lr(), odb::dbOrientType::MY);
  auto* sw = create_corner(
      "CORNER_SOUTH_WEST", corner_origins.ll(), odb::dbOrientType::R0);

  // Create rows
  auto create_row = [block, &name_format, &xform](const std::string& name,
                                                  odb::dbSite* site,
                                                  int sites,
                                                  const odb::Point& origin,
                                                  odb::dbOrientType orient,
                                                  odb::dbRowDir direction) {
    const std::string row_name = fmt::format(name_format, name);
    odb::dbTransform rotation(orient);
    rotation.concat(xform);
    odb::dbRow::create(block,
                       row_name.c_str(),
                       site,
                       origin.x(),
                       origin.y(),
                       rotation.getOrient(),
                       direction,
                       sites,
                       site->getWidth());
  };
  create_row("NORTH",
             vertical_site,
             x_sites,
             {nw->getBBox().xMax(),
              outer_io.yMax() - static_cast<int>(vertical_site->getHeight())},
             odb::dbOrientType::MX,
             odb::dbRowDir::HORIZONTAL);
  create_row("EAST",
             horizontal_site,
             y_sites,
             {outer_io.xMax() - static_cast<int>(horizontal_site->getHeight()),
              se->getBBox().yMax()},
             odb::dbOrientType::R90,
             odb::dbRowDir::VERTICAL);
  create_row("SOUTH",
             vertical_site,
             x_sites,
             {sw->getBBox().xMax(), outer_io.yMin()},
             odb::dbOrientType::R0,
             odb::dbRowDir::HORIZONTAL);
  create_row("WEST",
             horizontal_site,
             y_sites,
             {outer_io.xMin(), sw->getBBox().yMax()},
             odb::dbOrientType::MXR90,
             odb::dbRowDir::VERTICAL);
}

void ICeWall::removeIORows()
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  for (auto* row : getRows()) {
    odb::dbRow::destroy(row);
  }
}

void ICeWall::placeCorner(odb::dbMaster* master, int ring_index)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  if (master == nullptr) {
    logger_->error(utl::PAD, 28, "Corner master must be specified.");
  }

  for (const char* row_name : {"IO_CORNER_NORTH_WEST",
                               "IO_CORNER_NORTH_EAST",
                               "IO_CORNER_SOUTH_WEST",
                               "IO_CORNER_SOUTH_EAST"}) {
    odb::dbRow* row;
    if (ring_index >= 0) {
      row = findRow(fmt::format("{}_{}", row_name, ring_index));
    } else {
      row = findRow(row_name);
    }
    if (row == nullptr) {
      logger_->warn(utl::PAD,
                    13,
                    "Unable to find {} row to place a corner cell in",
                    row_name);
    }

    const std::string corner_name = fmt::format("{}_INST", row->getName());
    odb::dbInst* inst = block->findInst(corner_name.c_str());
    if (inst == nullptr) {
      inst = odb::dbInst::create(block, master, corner_name.c_str());
    }

    const odb::Rect row_bbox = row->getBBox();

    inst->setOrient(row->getOrient());
    inst->setLocation(row_bbox.xMin(), row_bbox.yMin());
    inst->setPlacementStatus(odb::dbPlacementStatus::FIRM);
  }
}

void ICeWall::placePad(odb::dbMaster* master,
                       const std::string& name,
                       odb::dbRow* row,
                       int location,
                       bool mirror)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  odb::dbInst* inst = block->findInst(name.c_str());
  if (inst == nullptr) {
    if (master == nullptr) {
      logger_->error(
          utl::PAD, 18, "Unable to create instance {} without master", name);
    }
    inst = odb::dbInst::create(block, master, name.c_str());
  }

  if (row == nullptr) {
    logger_->error(utl::PAD, 19, "Row must be specified to place a pad");
  }

  odb::dbTransform orient(odb::dbOrientType::R0);
  if (mirror) {
    const odb::dbTransform mirror_transform(odb::dbOrientType::MY);
    orient.concat(mirror_transform);
  }

  placeInstance(row, snapToRowSite(row, location), inst, orient.getOrient());
}

int ICeWall::snapToRowSite(odb::dbRow* row, int location) const
{
  int x, y;
  row->getOrigin(x, y);
  const odb::Point origin(x, y);

  const double spacing = row->getSpacing();
  int relative_location;
  if (row->getDirection() == odb::dbRowDir::HORIZONTAL) {
    relative_location = location - origin.x();
  } else {
    relative_location = location - origin.y();
  }

  int site_count = std::round(relative_location / spacing);
  site_count = std::max(0, site_count);
  site_count = std::min(site_count, row->getSiteCount());

  return site_count;
}

odb::dbRow* ICeWall::findRow(const std::string& name) const
{
  for (auto* row : getBlock()->getRows()) {
    if (row->getName() == name) {
      return row;
    }
  }
  return nullptr;
}

void ICeWall::placeInstance(odb::dbRow* row,
                            int index,
                            odb::dbInst* inst,
                            odb::dbOrientType base_orient) const
{
  const int origin_offset = index * row->getSpacing();

  const odb::Rect row_bbox = row->getBBox();
  const std::string row_name = row->getName();

  odb::dbTransform xform(base_orient);
  xform.concat(row->getOrient());
  inst->setOrient(xform.getOrient());
  const odb::Rect inst_bbox = inst->getBBox()->getBox();

  odb::Point index_pt;
  if (row_name.find("NORTH") != std::string::npos) {
    index_pt = odb::Point(row_bbox.xMin() + origin_offset,
                          row_bbox.yMax() - inst_bbox.dy());
  } else if (row_name.find("SOUTH") != std::string::npos) {
    index_pt = odb::Point(row_bbox.xMin() + origin_offset, row_bbox.yMin());
  } else if (row_name.find("WEST") != std::string::npos) {
    index_pt = odb::Point(row_bbox.xMin(), row_bbox.yMin() + origin_offset);
  } else if (row_name.find("EAST") != std::string::npos) {
    index_pt = odb::Point(row_bbox.xMax() - inst_bbox.dx(),
                          row_bbox.yMin() + origin_offset);
  } else {
  }

  inst->setLocation(index_pt.x(), index_pt.y());

  // check for overlaps
  const odb::Rect inst_rect = inst->getBBox()->getBox();
  auto* block = getBlock();
  for (auto* check_inst : block->getInsts()) {
    if (check_inst == inst) {
      continue;
    }
    if (!check_inst->isFixed()) {
      continue;
    }
    const odb::Rect check_rect = check_inst->getBBox()->getBox();
    if (inst_rect.overlaps(check_rect)) {
      const double dbus = block->getDbUnitsPerMicron();
      logger_->error(utl::PAD,
                     1,
                     "Unable to place {} ({}) at ({:.3f}um, {:.3f}um) - "
                     "({:.3f}um, {:.3f}um) as it "
                     "overlaps with {} ({})",
                     inst->getName(),
                     inst->getMaster()->getName(),
                     inst_rect.xMin() / dbus,
                     inst_rect.yMin() / dbus,
                     inst_rect.xMax() / dbus,
                     inst_rect.yMax() / dbus,
                     check_inst->getName(),
                     check_inst->getMaster()->getName());
    }
  }
  inst->setPlacementStatus(odb::dbPlacementStatus::FIRM);
}

void ICeWall::placeFiller(const std::vector<odb::dbMaster*>& masters,
                          odb::dbRow* row)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  if (row == nullptr) {
    logger_->error(utl::PAD, 20, "Row must be specified to place IO filler");
  }

  const double dbus = block->getDbUnitsPerMicron();

  std::vector<odb::dbMaster*> fillers = masters;
  // remove nullptrs
  fillers.erase(std::remove(fillers.begin(), fillers.end(), nullptr),
                fillers.end());
  // sort by width
  std::stable_sort(fillers.begin(),
                   fillers.end(),
                   [](odb::dbMaster* r, odb::dbMaster* l) -> bool {
                     // sort biggest to smallest
                     return r->getWidth() > l->getWidth();
                   });

  const odb::Rect rowbbox = row->getBBox();

  using namespace boost::polygon::operators;
  using Rectangle = boost::polygon::rectangle_data<int>;
  using Polygon90 = boost::polygon::polygon_90_with_holes_data<int>;
  using Polygon90Set = boost::polygon::polygon_90_set_data<int>;
  using Pt = Polygon90::point_type;

  std::vector<Polygon90> placed_io;
  for (auto* inst : getPadInstsInRow(row)) {
    const odb::Rect bbox = inst->getBBox()->getBox();
    std::array<Pt, 4> pts;
    if (row->getDirection() == odb::dbRowDir::HORIZONTAL) {
      pts = {Pt(bbox.xMin(), rowbbox.yMin()),
             Pt(bbox.xMax(), rowbbox.yMin()),
             Pt(bbox.xMax(), rowbbox.yMax()),
             Pt(bbox.xMin(), rowbbox.yMax())};
    } else {
      pts = {Pt(rowbbox.xMin(), bbox.yMin()),
             Pt(rowbbox.xMax(), bbox.yMin()),
             Pt(rowbbox.xMax(), bbox.yMax()),
             Pt(rowbbox.xMin(), bbox.yMax())};
    }

    debugPrint(
        logger_,
        utl::PAD,
        "Fill",
        2,
        "Instance in {} -> {} ({:.3f}um, {:.3f}um) -> ({:.3f}um, {:.3f}um)",
        row->getName(),
        inst->getName(),
        pts[0].x() / dbus,
        pts[0].y() / dbus,
        pts[2].x() / dbus,
        pts[2].x() / dbus);

    Polygon90 poly;
    poly.set(pts.begin(), pts.end());
    placed_io.push_back(poly);
  }

  std::array<Pt, 4> pts = {Pt(rowbbox.xMin(), rowbbox.yMin()),
                           Pt(rowbbox.xMax(), rowbbox.yMin()),
                           Pt(rowbbox.xMax(), rowbbox.yMax()),
                           Pt(rowbbox.xMin(), rowbbox.yMax())};

  Polygon90 poly;
  poly.set(pts.begin(), pts.end());
  std::array<Polygon90, 1> arr{poly};

  Polygon90Set new_shape(boost::polygon::HORIZONTAL, arr.begin(), arr.end());

  for (const auto& io : placed_io) {
    new_shape -= io;
  }

  std::vector<Rectangle> rects;
  new_shape.get_rectangles(rects);

  const int site_width = row->getSite()->getWidth();
  int fill_group = 0;
  for (auto& r : rects) {
    const odb::Rect new_rect(xl(r), yl(r), xh(r), yh(r));

    int width;
    int start;
    if (row->getDirection() == odb::dbRowDir::HORIZONTAL) {
      width = new_rect.dx();
      start = new_rect.xMin();
    } else {
      width = new_rect.dy();
      start = new_rect.yMin();
    }
    int sites = width / site_width;
    const int start_site_index = snapToRowSite(row, start);

    debugPrint(logger_,
               utl::PAD,
               "Fill",
               1,
               "Filling {} ({:.3f}um, {:.3f}um) -> ({:.3f}um, {:.3f}um)",
               row->getName(),
               new_rect.xMin() / dbus,
               new_rect.yMin() / dbus,
               new_rect.xMax() / dbus,
               new_rect.yMax() / dbus);
    debugPrint(logger_,
               utl::PAD,
               "Fill",
               2,
               "  start index {} width {}",
               start_site_index,
               sites);

    int site_offset = 0;
    for (auto* filler : fillers) {
      const int fill_width = filler->getWidth() / site_width;
      while (fill_width <= sites) {
        debugPrint(logger_,
                   utl::PAD,
                   "Fill",
                   2,
                   "    fill cell {} width {} remaining sites {}",
                   filler->getName(),
                   fill_width,
                   sites);

        const std::string name = fmt::format(
            "IO_FILL_{}_{}_{}", row->getName(), fill_group, site_offset);
        auto* fill_inst = odb::dbInst::create(block, filler, name.c_str());

        placeInstance(row,
                      start_site_index + site_offset,
                      fill_inst,
                      odb::dbOrientType::R0);

        site_offset += fill_width;
        sites -= fill_width;
      }
    }
    fill_group++;
  }
}

void ICeWall::removeFiller(odb::dbRow* row)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  if (row == nullptr) {
    logger_->error(utl::PAD, 21, "Row must be specified to remove IO filler");
  }

  const std::string prefix = fmt::format("IO_FILL_{}_", row->getName());

  for (auto* inst : block->getInsts()) {
    const std::string name = inst->getName();
    if (name.substr(0, prefix.length()) == prefix) {
      odb::dbInst::destroy(inst);
    }
  }
}

void ICeWall::placeBondPads(odb::dbMaster* bond,
                            const std::vector<odb::dbInst*>& pads,
                            odb::dbOrientType rotation,
                            const odb::Point& offset,
                            const std::string& prefix)
{
  auto* block = getBlock();
  if (block == nullptr) {
    return;
  }

  if (bond == nullptr) {
    logger_->error(
        utl::PAD, 27, "Bond master must be specified to place bond pads");
  }

  assertMasterType(bond, odb::dbMasterType::COVER);

  odb::dbMTerm* bond_pin = nullptr;
  odb::Rect bond_rect;
  odb::dbTechLayer* bond_layer = nullptr;
  for (auto* mterm : bond->getMTerms()) {
    for (auto* mpin : mterm->getMPins()) {
      for (auto* geom : mpin->getGeometry()) {
        auto* pin_layer = geom->getTechLayer();
        if (pin_layer == nullptr) {
          continue;
        }
        if (pin_layer->getRoutingLevel() == 0) {
          continue;
        }
        bond_layer = pin_layer;
        bond_pin = mterm;
        bond_rect = geom->getBox();
      }
    }
  }

  const odb::dbTransform pad_xform(rotation);

  for (auto* inst : pads) {
    if (inst == nullptr) {
      continue;
    }
    if (!inst->isFixed()) {
      continue;
    }

    odb::dbTransform pad_transform(inst->getOrient());
    odb::Point pad_offset = offset;
    pad_transform.apply(pad_offset);
    int x, y;
    inst->getOrigin(x, y);
    const odb::Point pad_loc(x + pad_offset.x(), y + pad_offset.y());

    pad_transform.concat(pad_xform);
    const odb::dbOrientType pad_orient = pad_transform.getOrient();

    const std::string name = fmt::format("{}{}", prefix, inst->getName());

    odb::dbInst* bond_inst = odb::dbInst::create(block, bond, name.c_str());
    bond_inst->setOrient(pad_orient);
    bond_inst->setOrigin(pad_loc.x(), pad_loc.y());
    bond_inst->setPlacementStatus(odb::dbPlacementStatus::FIRM);

    // connect bond and pad
    odb::dbTransform xform;
    bond_inst->getTransform(xform);
    odb::Rect pin_shape = bond_rect;
    xform.apply(pin_shape);
    for (auto* iterm : inst->getITerms()) {
      auto* net = iterm->getNet();
      if (net == nullptr) {
        continue;
      }
      auto* mterm = iterm->getMTerm();
      for (auto* mpin : mterm->getMPins()) {
        for (auto* geom : mpin->getGeometry()) {
          auto* pin_layer = geom->getTechLayer();
          if (pin_layer == bond_layer) {
            bond_inst->getITerm(bond_pin)->connect(net);
            makeBTerm(net, pin_layer, pin_shape);
          }
        }
      }
    }
  }
}

void ICeWall::connectByAbutment()
{
  const std::vector<odb::dbInst*> io_insts = getPadInsts();

  debugPrint(logger_,
             utl::PAD,
             "Connect",
             1,
             "Connecting {} instances by abutment",
             io_insts.size());

  // Collect all touching iterms
  std::vector<std::pair<odb::dbITerm*, odb::dbITerm*>> connections;
  for (size_t i = 0; i < io_insts.size(); i++) {
    auto* inst0 = io_insts[i];
    for (size_t j = i + 1; j < io_insts.size(); j++) {
      auto* inst1 = io_insts[j];
      const auto inst_connections = getTouchingIterms(inst0, inst1);
      connections.insert(
          connections.end(), inst_connections.begin(), inst_connections.end());
    }
  }
  debugPrint(logger_,
             utl::PAD,
             "Connect",
             1,
             "{} touching iterms found",
             connections.size());

  // begin connections for current signals
  std::set<odb::dbNet*> special_nets = connectByAbutment(connections);

  // make nets for newly formed nets
  for (const auto& [iterm0, iterm1] : connections) {
    auto* net = iterm0->getNet();
    if (net == nullptr) {
      const std::string netname = fmt::format("{}.{}_RING",
                                              iterm0->getInst()->getName(),
                                              iterm0->getMTerm()->getName());
      odb::dbNet* new_net = odb::dbNet::create(getBlock(), netname.c_str());
      iterm0->connect(new_net);
      iterm1->connect(new_net);

      const auto new_nets = connectByAbutment(connections);
      special_nets.insert(new_nets.begin(), new_nets.end());
    }
  }

  for (auto* net : special_nets) {
    Utilities::makeSpecial(net);
  }
}

std::set<odb::dbNet*> ICeWall::connectByAbutment(
    const std::vector<std::pair<odb::dbITerm*, odb::dbITerm*>>& connections)
    const
{
  std::set<odb::dbNet*> special_nets;
  bool changed = false;
  int iter = 0;
  do {
    changed = false;
    debugPrint(logger_,
               utl::PAD,
               "Connect",
               1,
               "Start of connecting iteration {}",
               iter);

    for (const auto& [iterm0, iterm1] : connections) {
      auto* net0 = iterm0->getNet();
      auto* net1 = iterm1->getNet();

      if (net0 == net1) {
        continue;
      }

      if (net0 != nullptr && net1 != nullptr) {
        // ERROR, touching, but different nets
        logger_->error(utl::PAD,
                       2,
                       "{}/{} ({}) and {}/{} ({}) are touching, but are "
                       "connected to different nets",
                       iterm0->getInst()->getName(),
                       iterm0->getMTerm()->getName(),
                       net0->getName(),
                       iterm1->getInst()->getName(),
                       iterm1->getMTerm()->getName(),
                       net1->getName());
      }

      auto* connect_net = net0;
      if (connect_net == nullptr) {
        connect_net = net1;
      }

      debugPrint(logger_,
                 utl::PAD,
                 "Connect",
                 1,
                 "Connecting net {} to {}/{} ({}) and {}/{} ({})",
                 connect_net->getName(),
                 iterm0->getInst()->getName(),
                 iterm0->getMTerm()->getName(),
                 net0 != nullptr ? net0->getName() : "NULL",
                 iterm1->getInst()->getName(),
                 iterm1->getMTerm()->getName(),
                 net1 != nullptr ? net1->getName() : "NULL");

      if (net0 != connect_net) {
        iterm0->connect(connect_net);
        special_nets.insert(connect_net);
        changed = true;
      }
      if (net1 != connect_net) {
        iterm1->connect(connect_net);
        special_nets.insert(connect_net);
        changed = true;
      }
    }
    iter++;
  } while (changed);

  return special_nets;
}

std::vector<std::pair<odb::dbITerm*, odb::dbITerm*>> ICeWall::getTouchingIterms(
    odb::dbInst* inst0,
    odb::dbInst* inst1) const
{
  if (!inst0->getBBox()->getBox().intersects(inst1->getBBox()->getBox())) {
    return {};
  }

  using ShapeMap = std::map<odb::dbTechLayer*, std::set<odb::Rect>>;
  auto populate_map = [](odb::dbITerm* iterm) -> ShapeMap {
    ShapeMap map;
    odb::dbTransform xform;
    iterm->getInst()->getTransform(xform);

    for (auto* mpin : iterm->getMTerm()->getMPins()) {
      for (auto* geom : mpin->getGeometry()) {
        auto* layer = geom->getTechLayer();
        if (layer == nullptr) {
          continue;
        }
        odb::Rect shape = geom->getBox();
        xform.apply(shape);
        map[layer].insert(shape);
      }
    }
    return map;
  };

  std::map<odb::dbITerm*, ShapeMap> iterm_map;
  for (auto* iterm : inst0->getITerms()) {
    iterm_map[iterm] = populate_map(iterm);
  }
  for (auto* iterm : inst1->getITerms()) {
    iterm_map[iterm] = populate_map(iterm);
  }

  std::set<std::pair<odb::dbITerm*, odb::dbITerm*>> connections;
  for (auto* iterm0 : inst0->getITerms()) {
    const ShapeMap& shapes0 = iterm_map[iterm0];
    for (auto* iterm1 : inst1->getITerms()) {
      const ShapeMap& shapes1 = iterm_map[iterm1];

      for (const auto& [layer, shapes] : shapes0) {
        auto find_layer = shapes1.find(layer);
        if (find_layer == shapes1.end()) {
          continue;
        }
        const auto& other_shapes = find_layer->second;

        for (const auto& rect0 : shapes) {
          for (const auto& rect1 : other_shapes) {
            if (rect0.intersects(rect1) || rect1.intersects(rect0)) {
              connections.insert({iterm0, iterm1});
            }
          }
        }
      }
    }
  }
  std::vector<std::pair<odb::dbITerm*, odb::dbITerm*>> conns(
      connections.begin(), connections.end());
  return conns;
}

std::vector<odb::dbRow*> ICeWall::getRows() const
{
  auto* block = getBlock();
  if (block == nullptr) {
    return {};
  }

  std::vector<odb::dbRow*> rows;

  for (auto* row : block->getRows()) {
    const std::string name = row->getName();

    if (name.substr(0, 3) == "IO_") {
      rows.push_back(row);
    }
  }

  return rows;
}

std::vector<odb::dbInst*> ICeWall::getPadInstsInRow(odb::dbRow* row) const
{
  auto* block = getBlock();
  if (block == nullptr) {
    return {};
  }

  if (row == nullptr) {
    return {};
  }

  std::vector<odb::dbInst*> insts;
  const odb::Rect row_bbox = row->getBBox();

  for (auto* inst : block->getInsts()) {
    if (!inst->isPlaced()) {
      continue;
    }

    if (inst->getMaster()->isCover()) {
      continue;
    }

    const odb::Rect instbbox = inst->getBBox()->getBox();

    if (row_bbox.intersects(instbbox)) {
      insts.push_back(inst);
    }
  }

  return insts;
}

std::vector<odb::dbInst*> ICeWall::getPadInsts() const
{
  std::vector<odb::dbInst*> insts;

  for (auto* row : getRows()) {
    const auto row_insts = getPadInstsInRow(row);

    debugPrint(logger_,
               utl::PAD,
               "Insts",
               1,
               "Found {} instances in {}",
               row_insts.size(),
               row->getName());

    insts.insert(insts.end(), row_insts.begin(), row_insts.end());
  }

  return insts;
}

void ICeWall::routeRDL(odb::dbTechLayer* layer,
                       odb::dbTechVia* via,
                       const std::vector<odb::dbNet*>& nets,
                       int width,
                       int spacing,
                       bool allow45)
{
  if (layer == nullptr) {
    logger_->error(utl::PAD, 22, "Layer must be specified to perform routing.");
  }

  router_ = std::make_unique<RDLRouter>(
      logger_, getBlock(), layer, via, width, spacing, allow45);
  if (router_gui_ != nullptr) {
    router_gui_->setRouter(router_.get());
  }
  router_->route(nets);
  if (router_gui_ != nullptr) {
    router_gui_->redraw();
  }
}

void ICeWall::routeRDLDebugGUI(bool enable)
{
  if (router_ == nullptr) {
    return;
  }

  if (enable) {
    if (router_gui_ == nullptr) {
      router_gui_ = std::make_unique<RDLGui>();
      if (router_ != nullptr) {
        router_gui_->setRouter(router_.get());
      }
    }
    gui::Gui::get()->registerRenderer(router_gui_.get());
  } else {
    gui::Gui::get()->unregisterRenderer(router_gui_.get());
  }
}

}  // namespace pad
