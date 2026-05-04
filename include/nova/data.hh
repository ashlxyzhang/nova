#pragma once

/**
 * @file data.hh
 * @brief NOVA Data Acquisition Module
 * 
 * Provides interfaces for reading and writing event camera data
 * from various sources (files, live cameras).
 */

#include "render/GPUDevice.hh"
#include "data/DataSource.hh"
#include "data/DataAcquisition.hh"
#include "data/Scrubber.hh"
#include "data/EventData.hh"
#include "data/IEventReader.hh"
#include "data/IEventWriter.hh"
#include "data/DVEventReader.hh"
#include "data/DVEventWriter.hh"
#include "data/MetavisionEventReader.hh"