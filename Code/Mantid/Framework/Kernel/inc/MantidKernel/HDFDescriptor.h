#ifndef MANTID_KERNEL_HIERARCHICALFILEDESCRIPTOR_H_
#define MANTID_KERNEL_HIERARCHICALFILEDESCRIPTOR_H_

#include "MantidKernel/ClassMacros.h"
#include "MantidKernel/DllConfig.h"

#include <map>
#include <string>

namespace Mantid
{
  namespace Kernel
  {

    /** 
        Defines a wrapper around a file whose internal structure is stored in a hierarchy, e.g NeXus.

        On construction the simple details about the layout of the file are cached for faster querying later.

        Copyright &copy; 2013 ISIS Rutherford Appleton Laboratory & NScD Oak Ridge National Laboratory

        This file is part of Mantid.

        Mantid is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 3 of the License, or
        (at your option) any later version.

        Mantid is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.  If not, see <http://www.gnu.org/licenses/>.

        File change history is stored at: <https://github.com/mantidproject/mantid>
        Code Documentation is available at: <http://doxygen.mantidproject.org>
     */
    class MANTID_KERNEL_DLL HDFDescriptor
    {
    public:
      /// Enumerate HDF possible versions
      enum Version { Version4, Version5, AnyVersion };

      static const size_t HDFMagicSize;
      /// HDF cookie that is stored in the first 4 bytes of the file.
      static const unsigned char HDFMagic[4];
      /// Size of HDF5 signature
      static size_t HDF5SignatureSize;
      /// signature identifying a HDF5 file.
      static const unsigned char HDF5Signature[8];

      /// Returns true if the file is considered to store data in a hierarchy
      static bool isHDF(const std::string & filename, const Version version = AnyVersion);

    public:
      /// Constructor accepting a filename
      HDFDescriptor(const std::string & filename);
      /// Destructor
      ~HDFDescriptor();

      /**
       * Access the filename
       * @returns A reference to a const string containing the filename
       */
      inline const std::string & filename() const { return m_filename; }
      /**
       * Access the file extension. Defined as the string after and including the last period character
       * @returns A reference to a const string containing the file extension
       */
      inline const std::string & extension() const { return m_extension; }

      /// Query if a path exists
      bool pathExists(const std::string& path) const;
      /// Query if a given type exists somewhere in the file
      bool classTypeExists(const std::string & classType) const;

    private:
      DISABLE_DEFAULT_CONSTRUCT(HDFDescriptor);
      DISABLE_COPY_AND_ASSIGN(HDFDescriptor);

      /// Initialize object with filename
      void initialize(const std::string& filename);

      /// Full filename
      std::string m_filename;
      /// Extension
      std::string m_extension;
      /// Map of types to full path strings.
      std::multimap<std::string, std::string> *m_typesToPaths;
    };


  } // namespace Kernel
} // namespace Mantid

#endif  /* MANTID_KERNEL_HIERARCHICALFILEDESCRIPTOR_H_ */
