#if !defined(CAUSAL_RUNTIME_ELF_H)
#define CAUSAL_RUNTIME_ELF_H

#include <elf.h>

#include <map>

#include "arch.h"
#include "interval.h"
#include "util.h"

using std::map;
using std::pair;

// We're going to examine libraries linked with the currently running program, so the architecture
// is always going to be the same. Define header, section header, and symbol types for convenience.
_X86(typedef Elf32_Ehdr ELFHeader);
_X86(typedef Elf32_Shdr ELFSectionHeader);
_X86(typedef Elf32_Sym ELFSymbol);
_X86_64(typedef Elf64_Ehdr ELFHeader);
_X86_64(typedef Elf64_Shdr ELFSectionHeader);
_X86_64(typedef Elf64_Sym ELFSymbol);

#if _IS_X86
# define ELFSymbolType(x) ELF32_ST_TYPE(x)
#else
# define ELFSymbolType(x) ELF64_ST_TYPE(x)
#endif

static map<interval, string> getFunctions(const char* filename, void* load_address) {
  map<interval, string> functions;
  
  // Open the loaded file from disk
  int fd = open(filename, O_RDONLY);
  REQUIRE(fd != -1, "Failed to open file %s", filename);
  
  // Get file size
  struct stat sb;
  REQUIRE(fstat(fd, &sb) != -1, "Failed to get size for file %s", filename);
  
  // Map the ELF file
  void* base = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  REQUIRE(base != MAP_FAILED, "Failed to map file %s", filename);
  
  // Validate the ELF header
  Elf64_Ehdr* header = (Elf64_Ehdr*)base;
  if(header->e_ident[0] == 0x7f && header->e_ident[1] == 'E' &&
     header->e_ident[2] == 'L' && header->e_ident[3] == 'F') {

     ELFSectionHeader* sections = (ELFSectionHeader*)((uintptr_t)header + header->e_shoff);
     REQUIRE(header->e_shentsize == sizeof(ELFSectionHeader), "ELF section header size does not match loaded file");

     // Get the number of section headers
     size_t section_count = header->e_shnum;
     if(section_count == 0)
       section_count = sections->sh_size;

     // Loop over section headers
     for(ELFSectionHeader& section : wrap(sections, section_count)) {
       // Is this a symbol table section?
       if(section.sh_type == SHT_SYMTAB || section.sh_type == SHT_DYNSYM) {
         // Get the corresponding string table section header
         ELFSectionHeader& strtab_section = sections[section.sh_link];
         const char* strtab = (const char*)((uintptr_t)header + strtab_section.sh_offset);
    
         // Validate the symbol entry size
         REQUIRE(section.sh_entsize == sizeof(ELFSymbol), "ELF symbol size does not match loaded file");
    
         // Get the base pointer to this section's data
         ELFSymbol* symbols = (ELFSymbol*)((uintptr_t)header + section.sh_offset);
    
         // Loop over symbols in this section
         for(ELFSymbol& symbol : wrap(symbols, section.sh_size / sizeof(ELFSymbol))) {
           // Only handle function symbols with a defined value
           if(ELFSymbolType(symbol.st_info) == STT_FUNC && symbol.st_value != 0) {
             const char* name = strtab + symbol.st_name;
             interval range(symbol.st_value, symbol.st_value + symbol.st_size);
             if(header->e_type == ET_DYN)
               range += (uintptr_t)load_address;
        
             functions.insert(pair<const interval, string>(range, name));
           }
         }
       }
     }
  } else {
    WARNING("Not an elf file. Found magic bytes %#x %c%c%c",
      header->e_ident[0], header->e_ident[1], header->e_ident[2], header->e_ident[3]);
  }
  
  // Unmap and close
  REQUIRE(munmap(base, sb.st_size) != -1, "Failed to unmap file %s", filename);
  REQUIRE(close(fd) != -1, "Failed to close file %s", filename);
  
  return functions;
}

#endif
