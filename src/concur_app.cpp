#include "concur_app.hpp"
#include <be/cli/cli.hpp>
#include <be/util/files.hpp>
#include <be/util/path_glob.hpp>
#include <be/util/parse_numeric_string.hpp>
#include <be/core/logging.hpp>
#include <be/core/alg.hpp>
#include <gli/gli.hpp>
#include <stb/stb_image.h>
#include <iostream>
#include <fstream>

namespace be {
namespace concur {

///////////////////////////////////////////////////////////////////////////////
ConcurApp::ConcurApp(int argc, char** argv) {
   default_log().verbosity_mask(v::info_or_worse);
   try {
      using namespace cli;
      using namespace color;
      using namespace ct;
      Processor proc;

      glm::vec2 hotspot;

      bool show_version = false;
      bool show_help = false;
      S help_query;

      proc
         (prologue (Table() << header << "CONCUR .ICO/.CUR GENERATOR").query())

         (synopsis (Cell() << fg_dark_gray << "[ " << fg_cyan << "OPTIONS"
          << fg_dark_gray << " ] " << fg_cyan << "OUTPUT_PATH"))

         (abstract ("Concur converts one or more image files into a Windows icon or cursor."))

         (param ({ "I", "i" },{ "input" }, "PATH", 
            [&](const S& str) {
               inputs_[str] = input_type::automatic;
            }).desc(Cell() << "Adds the specified path as a source image.")
              .extra(Cell() << nl << "Adding an image does not guarantee that it will be used; use " << fg_yellow << "-s" << reset << " to specify an output image of the same or smaller size."
                            << "If the image is a PNG image, it will be stored as such in the icon or cursor, even if it is resized.  Otherwise it will be stored as a bitmap."))

         (param ({ "P", "p" },{ "png" }, "PATH", 
            [&](const S& str) {
               inputs_[str] = input_type::png;
            }).desc(Cell() << "Adds the specified path as a source image.  Output images based on this one will be stored as PNGs.")
               .extra(Cell() << nl << "Adding an image does not guarantee that it will be used; use " << fg_yellow << "-s" << reset << " to specify an output image of the same or smaller size."))

         (param ({ "B", "b" },{ "bmp", "dib" }, "PATH", 
            [&](const S& str) {
               inputs_[str] = input_type::bitmap;
            }).desc(Cell() << "Adds the specified path as a source image.  Output images based on this one will be stored as bitmaps.")
               .extra(Cell() << nl << "Adding an image does not guarantee that it will be used; use " << fg_yellow << "-s" << reset << " to specify an output image of the same or smaller size."))


         (param ({ "x" },{ "hotspot-x" }, "NUMBER",
            [&](const S& str) {
               hotspot.x = util::parse_bounded_numeric_string<F32>(str, 0, 1);
            }).desc(Cell() << "Specifies the X coordinate of the cursor hotspot.")
              .extra(Cell() << nl << "This option causes the output to be a cursor, regardless of the extension of the output file.  "
                            << "This option must be specified before any " << fg_yellow << "-s" << reset << " flags that define output sizes.  "
                            << "The number can be either a normalized floating-point value in the range [0, 1] or an integer ratio like " << fg_cyan << "4/16"))

          (param ({ "y" },{ "hotspot-y" }, "NUMBER",
           [&](const S& str) {
               hotspot.y = util::parse_bounded_numeric_string<F32>(str, 0, 1);
            }).desc(Cell() << "Specifies the Y coordinate of the cursor hotspot.")
               .extra(Cell() << nl << "This option causes the output to be a cursor, regardless of the extension of the output file.  "
                      << "This option must be specified before any " << fg_yellow << "-s" << reset << " flags that define output sizes.  "
                      << "The number can be either a normalized floating-point value in the range [0, 1] or an integer ratio like " << fg_cyan << "4/16"))

         (param ({ "s" },{ "size" }, "DIMENSION",
            [&](const S& str) {
               U16 size = util::parse_bounded_numeric_string<U16>(str, 1, 256);
               output_sizes_[size] = hotspot;
            }).desc(Cell() << "An image of the specified width and height will be added to the output.")
              .extra(Cell() << nl << "If no source image is specified with this size or larger, a warning will be generated and this image size will be skipped."))

         (flag ({ "S" },{ "small", "16" }, "Equivalent to -s 16",
            [&]() {
               output_sizes_[16] = hotspot;
            }))

         (flag ({ "M" },{ "medium", "24" }, "Equivalent to -s 24",
            [&]() {
               output_sizes_[24] = hotspot;
            }))

         (flag ({ "N" },{ "normal", "32" }, "Equivalent to -s 32",
            [&]() {
               output_sizes_[32] = hotspot;
            }))

         (flag ({ "L" },{ "large", "48" }, "Equivalent to -s 48",
            [&]() {
               output_sizes_[48] = hotspot;
            }))

         (flag ({ "X" },{ "extra-large", "256" }, "Equivalent to -s 256",
            [&]() {
               output_sizes_[256] = hotspot;
            }))

         (flag ({ "A" },{ "all" }, "Equivalent to -SMNLX",
            [&]() {
               output_sizes_[16] = hotspot;
               output_sizes_[24] = hotspot;
               output_sizes_[32] = hotspot;
               output_sizes_[48] = hotspot;
               output_sizes_[256] = hotspot;
            }))
         
         (nth (0,
            [&](const S& str) {
               output_path_ = str;
               return true;
            }))

         (end_of_options ())

         (verbosity_param ({ "v" },{ "verbosity" }, "LEVEL", default_log().verbosity_mask()))
         
         (flag ({ "V" },{ "version" }, "Prints version information to standard output.", show_version))

         (param ({ "?" },{ "help" }, "OPTION",
            [&](const S& value) {
               show_help = true;
               help_query = value;
            }).default_value(S())
              .allow_options_as_values(true)
              .desc(Cell() << "Outputs this help message.  For more verbose help, use " << fg_yellow << "--help")
              .extra(Cell() << nl << "If " << fg_cyan << "OPTION" << reset
                            << " is provided, the options list will be filtered to show only options that contain that string."))

         (flag ({ },{ "help" },
            [&]() {
               proc.verbose(true);
            }).ignore_values(true))
               
         (exit_code (0, "There were no errors."))
         (exit_code (1, "An unknown error occurred."))
         (exit_code (2, "There was a problem parsing the command line arguments."))
         (exit_code (3, "An input file does not exist or is a directory."))
         (exit_code (4, "An I/O error occurred while reading an input file."))
         (exit_code (5, "An I/O error occurred while writing an output file."))
               
         (example (Cell() << fg_gray << "icon.ico" << fg_yellow << " -i " << fg_cyan << "icon_image.tga" << fg_yellow << " -A",
            "Creates an icon named 'icon.ico' in the working directory containing 16x16, 24x24, 32x32, 48x48, and 256x256 bitmap images, assuming icon_image.tga is at least 256 pixels wide/high."))
         (example (Cell() << fg_yellow << "-b " << fg_cyan << "icon_16x16.png"
                          << fg_yellow << " -b " << fg_cyan << "icon_64x64.png"
                          << fg_yellow << " -i " << fg_cyan << "icon_256x256.png"
                          << fg_yellow << " -SNX -s " << fg_cyan << "128" << fg_gray << " icon.ico",
            "Creates an icon from 3 input images of different resolutions.  The output icon will have 4 different sizes: 16x16 (bitmap), 32x32 (bitmap), 128x128 (png), and 256x256 (png)."))
         (example (Cell() << fg_yellow << "-i " << fg_cyan << "icon_image.tga"
                          << fg_yellow << " -xy " << fg_cyan << "2/16"
                          << fg_yellow << " -SM -x " << fg_cyan << "3/32"
                          << fg_yellow << " -N " << fg_gray << "cursor.cur",
            "Creates an icon with 16x16, 24x24, and 32x32 sizes from a single input image, resized.  The 16x16 image has the hotspot at 2,2, the 24x24 image has it at 3,3, and the 32x32 image has it at 3,4."))

         ;

      proc(argc, argv);

      if (!show_help && !show_version && output_path_.empty()) {
         show_help = true;
         show_version = true;
         status_ = 1;
      }

      if (show_version) {
         proc
            //(prologue (BE_BLT_VERSION).query())
            (license (BE_LICENSE).query())
            (license (BE_COPYRIGHT).query())
            ;
      }

      if (show_help) {
         proc.describe(std::cout, help_query);
      } else if (show_version) {
         proc.describe(std::cout, ids::cli_describe_section_prologue);
         proc.describe(std::cout, ids::cli_describe_section_license);
      }

   } catch (const cli::OptionException& e) {
      status_ = 2;
      be_error() << S(e.what())
         & attr(ids::log_attr_index) << e.raw_position()
         & attr(ids::log_attr_argument) << S(e.argument())
         & attr(ids::log_attr_option) << S(e.option())
         | default_log();
   } catch (const cli::ArgumentException& e) {
      status_ = 2;
      be_error() << S(e.what())
         & attr(ids::log_attr_index) << e.raw_position()
         & attr(ids::log_attr_argument) << S(e.argument())
         | default_log();
   } catch (const Fatal& e) {
      status_ = 2;
      be_error() << "Fatal error while parsing command line!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const Recoverable<>& e) {
      status_ = 2;
      be_error() << "Error while parsing command line!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const std::exception& e) {
      status_ = 2;
      be_error() << "Unexpected exception parsing command line!"
         & attr(ids::log_attr_message) << S(e.what())
         | default_log();
   }   
}

///////////////////////////////////////////////////////////////////////////////
int ConcurApp::operator()() {
   if (status_ != 0) {
      return status_;
   }

   if (output_path_.empty()) {
      return status_;
   }

   struct ImageData {
      gli::image image;
      input_type type;
   };

   std::map<U16, ImageData> images;

   try {
      for (const auto& pair : inputs_) {
         const Path& path = pair.first;
         if (!fs::exists(path)) {
            status_ = 3;
            be_error() << "Input path does not exist!"
               & attr(ids::log_attr_path) << path
               | default_log();
         } else if (!fs::is_regular_file(path)) {
            status_ = 3;
            be_error() << "Input path is not a file!"
               & attr(ids::log_attr_path) << path
               | default_log();
         }

         


      }

      // TODO load inputs

   } catch (const fs::filesystem_error& e) {
      status_ = 4;
      be_error() << "Filesystem error while reading inputs!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_code) << std::error_code(e.code())
         & attr(ids::log_attr_path) << e.path1().generic_string()
         | default_log();
   } catch (const Fatal& e) {
      status_ = 4;
      be_error() << "Fatal error while reading inputs!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const Recoverable<>& e) {
      status_ = 4;
      be_error() << "Error while reading inputs!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const std::exception& e) {
      status_ = 4;
      be_error() << "Unexpected exception while reading inputs!"
         & attr(ids::log_attr_message) << S(e.what())
         | default_log();
   }

   // TODO resize images
   // TODO serialize output images
   // TODO optimize pngs if necessary

   try {
      output_path_ = fs::absolute(output_path_);
      if (fs::exists(output_path_)) {
         if (!fs::is_regular_file(output_path_)) {
            status_ = 5;
            be_error() << "Output path already exists and is not a file!"
               & attr(ids::log_attr_path) << output_path_
               | default_log();
         }
      } else if (!fs::exists(output_path_.parent_path())) {
         fs::create_directories(output_path_.parent_path());
      }

      be_short_verbose() << "Output path: " << color::fg_gray << BE_LOG_INTERP(BEIDN_LOG_ATTR_PATH)
         & hidden(ids::log_attr_path) << output_path_.generic_string()
         | default_log();





      
      
      // TODO write ico/cur
   } catch (const fs::filesystem_error& e) {
      status_ = 1;
      be_error() << "Filesystem error while configuring paths!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_code) << std::error_code(e.code())
         & attr(ids::log_attr_path) << e.path1().generic_string()
         | default_log();
   } catch (const Fatal& e) {
      status_ = 1;
      be_error() << "Fatal error while configuring paths!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const Recoverable<>& e) {
      status_ = 1;
      be_error() << "Error while configuring paths!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const std::exception& e) {
      status_ = 1;
      be_error() << "Unexpected exception while configuring paths!"
         & attr(ids::log_attr_message) << S(e.what())
         | default_log();
   }

   return status_;
}

} // be::concur
} // be
