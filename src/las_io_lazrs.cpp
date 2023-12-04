// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "PointArray.h"
#include "QtLogger.h"
#include "util.h"

#include <QRandomGenerator>
#include <lasreader_las.hpp>
#include <lazrs/lazrs_cpp.h>
#include <minilas/las.h>

bool PointArray::loadLas(QString fileName, size_t maxPointCount,
                         std::vector<GeomField> &fields, V3d &offset,
                         size_t &npoints, uint64_t &totalPoints) {

  las_file_t *las_file = las_file_open(fileName.toUtf8().constData());
  print_header(&las_file->header);

  const las_vlr *laszip_vlr = find_laszip_vlr(&las_file->header);
  if (laszip_vlr == nullptr) {
    g_logger.error("No laszip vlr found \"%s\"", fileName);
    return false;
  }

  lazrs::LasZipDecompressor decompressor(
      fileName.toStdString(), laszip_vlr->data, laszip_vlr->record_len,
      las_file->header.offset_to_point_data, true);
  std::vector<uint8_t> point_data(las_file->header.point_size *
                                      las_file->header.point_count *
                                      sizeof(uint8_t),
                                  0);
  decompressor.decompress_many(
      point_data.data(), las_file->header.point_size *
                             las_file->header.point_count * sizeof(uint8_t));
  printf("Decompressed %" PRIu64 "points\n", las_file->header.point_count);

  File file;
  std::unique_ptr<LASreaderLAS> lasReader(new LASreaderLAS());
#ifdef _WIN32
  file = _wfopen(fileName.toStdWString().data(), L"rb");
#else
  file = fopen(fileName.toUtf8().constData(), "rb");
#endif
  if (!file || !lasReader->open(file)) {
    g_logger.error("Couldn't open file \"%s\"", fileName);
    return false;
  }

  // std::ofstream dumpFile("points.txt");
  // Figure out how much to decimate the point cloud.
  totalPoints =
      std::max<uint64_t>(lasReader->header.extended_number_of_point_records,
                         lasReader->header.number_of_point_records);
  size_t decimate =
      totalPoints == 0 ? 1 : 1 + (totalPoints - 1) / maxPointCount;
  if (decimate > 1) {
    g_logger.info("Decimating \"%s\" by factor of %d", fileName.toStdString(),
                  decimate);
  }
  npoints = (totalPoints + decimate - 1) / decimate;
  offset = V3d(lasReader->header.min_x, lasReader->header.min_y, 0);
  // Attempt to place all data on the same vertical scale, but allow other
  // offsets if the magnitude of z is too large (and we would therefore loose
  // noticable precision by storing the data as floats)
  if (fabs(lasReader->header.min_z) > 10000)
    offset.z = lasReader->header.min_z;
  fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
  fields.push_back(GeomField(TypeSpec::uint16_i(), "intensity", npoints));
  fields.push_back(GeomField(TypeSpec::uint8_i(), "returnNumber", npoints));
  fields.push_back(GeomField(TypeSpec::uint8_i(), "numberOfReturns", npoints));
  fields.push_back(GeomField(TypeSpec::uint16_i(), "pointSourceId", npoints));
  fields.push_back(GeomField(TypeSpec::uint8_i(), "classification", npoints));
  if (totalPoints == 0) {
    g_logger.warning("File %s has zero points", fileName);
    return true;
  }
  // Iterate over all points & pull in the data.
  V3f *position = (V3f *)fields[0].as<float>();
  uint16_t *intensity = fields[1].as<uint16_t>();
  uint8_t *returnNumber = fields[2].as<uint8_t>();
  uint8_t *numReturns = fields[3].as<uint8_t>();
  uint16_t *pointSourceId = fields[4].as<uint16_t>();
  uint8_t *classification = fields[5].as<uint8_t>();
  uint64_t readCount = 0;
  uint64_t nextDecimateBlock = 1;
  uint64_t nextStore = 1;
  size_t storeCount = 0;
  QRandomGenerator rand;
  //   if (!lasReader->read_point())
  //     return false;
  //   const LASpoint &point = lasReader->point;
  LASpoint point;
  if (!point.init(&lasReader->header, lasReader->header.point_data_format,
                  lasReader->header.point_data_record_length,
                  &lasReader->header)) {
    g_logger.error("Couldn't init point \"%s\"", fileName);
    return false;
  }

  uint16_t *color = 0;
  if (point.have_rgb) {
    fields.push_back(GeomField(TypeSpec(TypeSpec::Uint, 2, 3, TypeSpec::Color),
                               "color", npoints));
    color = fields.back().as<uint16_t>();
  }
  // iterate over point_data using iterators
  uint8_t point_buffer[50];
  std::vector<uint8_t>::iterator it = point_data.begin();
  for (it = point_data.begin(); it < point_data.end();
       it += lasReader->header.point_data_record_length) {
    if (it + lasReader->header.point_data_record_length > point_data.end()) {
      g_logger.warning(
          ("Ran into end of point data! Current idx: " +
           std::to_string(it - point_data.begin()) +
           " Point data size: " + std::to_string(point_data.size()))
              .c_str());
      // g_logger.warning("Expected %d points in file \"%s\", got %d",
      // totalPoints,
      //                  fileName, readCount);
      break;
    }
    // copy 50 bytes from point_data into point_buffer
    std::copy(it, it + lasReader->header.point_data_record_length,
              point_buffer);
    point.copy_from(point_buffer);

    // Read a point from the las file
    ++readCount;
    if (readCount % 10000 == 0)
      emit loadProgress(100 * readCount / totalPoints);
    V3d P = V3d(point.get_x(), point.get_y(), point.get_z());
    if (readCount < nextStore)
      continue;
    ++storeCount;
    // Store the point
    *position++ = P - offset;
    // float intens = float(point.scan_angle_rank) / 40;
    *intensity++ = point.intensity;
    *returnNumber++ = point.return_number;
#if LAS_TOOLS_VERSION >= 140315
    *numReturns++ = point.number_of_returns;
#else
    *numReturns++ = point.number_of_returns_of_given_pulse;
#endif
    *pointSourceId++ = point.point_source_ID;

    if (point.extended_point_type) {
      *classification++ = point.extended_classification;
    } else {
      // Put flags back in classification byte to avoid memory bloat
      *classification++ = point.classification | (point.synthetic_flag << 5) |
                          (point.keypoint_flag << 6) |
                          (point.withheld_flag << 7);
    }

    // Extract point RGB
    if (color) {
      *color++ = point.rgb[0];
      *color++ = point.rgb[1];
      *color++ = point.rgb[2];
    }
    // Figure out which point will be the next stored point.
    nextDecimateBlock += decimate;
    nextStore = nextDecimateBlock;
    if (decimate > 1) {
      // Randomize selected point within block to avoid repeated patterns
      nextStore += (rand.generate64() % decimate);
      if (nextDecimateBlock <= totalPoints && nextStore > totalPoints)
        nextStore = totalPoints;
    }
  }
  lasReader->close();
  if (readCount < totalPoints) {
    g_logger.warning("Expected %d points in file \"%s\", got %d", totalPoints,
                     fileName, readCount);
    npoints = storeCount;
    // Shrink all fields to fit - these will have wasted space at the end,
    // but that will be fixed during reordering.
    for (size_t i = 0; i < fields.size(); ++i)
      fields[i].size = npoints;
    totalPoints = readCount;
  }
  return true;
}
