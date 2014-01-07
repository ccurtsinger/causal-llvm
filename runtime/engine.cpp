#include "engine.h"

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <papi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <set>
#include <string>

#include "function.h"
#include "interval.h"
#include "log.h"

using std::pair;
using std::set;
using std::string;

namespace engine {
  enum {
    CycleSamplePeriod = 10000000,
    CycleSampleMask = 0x1,
    InstructionSamplePeriod = 1000,
    InstructionSampleMask = 0x2
  };
  
  enum Mode {
    Idle,
    Sample,
    Slowdown,
    Speedup
  };
  
  pthread_mutex_t papi_mutex = PTHREAD_MUTEX_INITIALIZER;
  __thread int _event_set;
  Mode _mode = Idle;
  
  // Globals for straight-up sampling
  void** _cycle_samples;
  void** _inst_samples;
  size_t* _cycle_index;
  size_t* _inst_index;
  size_t _cycle_max;
  size_t _inst_max;
  
  void collectSamples(void** cycle_samples, size_t* cycle_max, void** inst_samples, size_t* inst_max) {
    _cycle_samples = cycle_samples;
    _inst_samples = inst_samples;
    
    _cycle_max = *cycle_max;
    _cycle_index = cycle_max;
    *_cycle_index = 0;
    
    _inst_max = *inst_max;
    _inst_index = inst_max;
    *_inst_index = 0;
    
    __atomic_store_n(&_mode, Sample, __ATOMIC_SEQ_CST);
    
    while(__atomic_load_n(&_mode, __ATOMIC_SEQ_CST) == Sample) {
      __asm__("pause");
    }
  }
  
  void overflowHandler(int event_set, void* address, long long vec, void* context) {
    Mode m = __atomic_load_n(&_mode, __ATOMIC_SEQ_CST);
    
    if(m == Idle) {
      // Do nothing
      
    } else if(m == Sample) {
      if(vec & CycleSampleMask) {
        size_t index = __atomic_fetch_add(_cycle_index, 1, __ATOMIC_SEQ_CST);
        if(index < _cycle_max) {
          _cycle_samples[index] = address;
        } else if(index == _cycle_max) {
          __atomic_store_n(&_mode, Idle, __ATOMIC_SEQ_CST);
        }
      }
  
      if(vec & InstructionSampleMask) {
        size_t index = __atomic_fetch_add(_inst_index, 1, __ATOMIC_SEQ_CST);
        if(index < _inst_max) {
          _inst_samples[index] = address;
        } else if(index == _cycle_max) {
          __atomic_store_n(&_mode, Idle, __ATOMIC_SEQ_CST);
        }
      }
    }
  }

  void initialize() {
    // Initialize the PAPI library
    REQUIRE(PAPI_library_init(PAPI_VER_CURRENT) == PAPI_VER_CURRENT, "Failed to initialize PAPI");
    REQUIRE(PAPI_thread_init(pthread_self) == PAPI_OK, "Failed initialize PAPI thread support");
    //REQUIRE(PAPI_set_domain(PAPI_DOM_ALL) == PAPI_OK, "Failed to set PAPI domain");
  
    // Verify that the instruction and cycle events are available
    REQUIRE(PAPI_query_event(PAPI_TOT_CYC) == PAPI_OK, "Hardware cycle counter is not available");
    REQUIRE(PAPI_query_event(PAPI_TOT_INS) == PAPI_OK, "Hardware instruction counter is not available");
      
    INFO("PAPI Initialized");
  }
  
  void addThread() {
    // Set up the PAPI event set
    _event_set = PAPI_NULL;
    REQUIRE(PAPI_create_eventset(&_event_set) == PAPI_OK, "Failed to create PAPI event set");
  
    // Add cycle and instruction counting events
    REQUIRE(PAPI_add_event(_event_set, PAPI_TOT_CYC) == PAPI_OK, "Failed to add cycle counter event");
    REQUIRE(PAPI_add_event(_event_set, PAPI_TOT_INS) == PAPI_OK, "Failed to add instruction counter event");
  
    // Set up sampling (overflow signals) for the cycle counter
    REQUIRE(PAPI_overflow(_event_set, PAPI_TOT_CYC, CycleSamplePeriod, 0, overflowHandler) == PAPI_OK, 
      "Failed to set up cycle counter sampling");
    
    // Set up sampling (overflow signals) for the instruction counter
    REQUIRE(PAPI_overflow(_event_set, PAPI_TOT_INS, InstructionSamplePeriod, 0, overflowHandler) == PAPI_OK,
      "Failed to set up instruction counter sampling");
    
    PAPI_start(_event_set);
  }
  
  void removeThread() {
    long long result[2];
    REQUIRE(PAPI_stop(_event_set, result) == PAPI_OK, "Failed to stop PAPI");
    REQUIRE(PAPI_cleanup_eventset(_event_set) == PAPI_OK, "Failed to clean up event set");
    REQUIRE(PAPI_destroy_eventset(&_event_set) == PAPI_OK, "Failed to destroy event set");
    REQUIRE(PAPI_unregister_thread() == PAPI_OK, "Failed to unregister thread");
  }
  
  // Process an ELF file with a known architecture. Caller should pass in types for
  // either ELF32 or ELF64 files. Field names and processing code are the same for both.
  template<bool Is64Bit, class ElfHeader, class SectionHeader, class Symbol>
  void processElfFile(const char* filename, ElfHeader* header, void* loaded) {
    // Get the first section header table entry
    SectionHeader* sh_entry = (SectionHeader*)((uintptr_t)header + header->e_shoff);
    
    // Get size and count information for section headers
    size_t sh_entry_size = header->e_shentsize;
    size_t sh_count = header->e_shnum;
    size_t shstr_index = header->e_shstrndx;
    
    // Handle large section header counts
    if(sh_count == 0) {
      sh_count = sh_entry->sh_size;
    }
    
    // Handle large indices for the section name table index
    if(shstr_index == SHN_XINDEX) {
      shstr_index = sh_entry->sh_link;
    }
    
    // Find the section section name table
    SectionHeader* shstr_entry = (SectionHeader*)((uintptr_t)sh_entry + sh_entry_size * shstr_index);
    REQUIRE(shstr_entry->sh_type == SHT_STRTAB, "Section name string table section is not a string table");
    const char* shstr_table = (const char*)((uintptr_t)header + shstr_entry->sh_offset);
    
    // Find the string table section header
    size_t sh_index = 0;
    SectionHeader* sh_strtab = NULL;
    while(sh_index < sh_count) {
      if(sh_entry->sh_type == SHT_STRTAB) {
        if(string(shstr_table + sh_entry->sh_name) == ".strtab") {
          sh_strtab = sh_entry;
        } else if(sh_strtab == NULL && string(shstr_table + sh_entry->sh_name) == ".dynstr") {
          sh_strtab = sh_entry;
        }
      }
      sh_index++;
      sh_entry = (SectionHeader*)((uintptr_t)sh_entry + sh_entry_size);
    }
    REQUIRE(sh_strtab != NULL, "Couldn't find string table in %s", filename);
    
    const char* string_table = (const char*)((uintptr_t)header + sh_strtab->sh_offset);
    
    INFO("%s", filename);
    sh_index = 0;
    sh_entry = (SectionHeader*)((uintptr_t)header + header->e_shoff);
    while(sh_index < sh_count) {
      // Is this a symbol table?
      if(sh_entry->sh_type == SHT_SYMTAB || sh_entry->sh_type == SHT_DYNSYM) {
        INFO(" Found symbol table in section %s", shstr_table + sh_entry->sh_name);
        
        size_t symbol_size = sh_entry->sh_entsize;
        size_t symbol_count = sh_entry->sh_size / symbol_size;
        
        Symbol* symbol = (Symbol*)((uintptr_t)header + sh_entry->sh_offset);
        
        size_t symbol_index = 0;
        while(symbol_index < symbol_count) {
          int type;
          
          if(Is64Bit)
            type = ELF64_ST_TYPE(symbol->st_info);
          else
            type = ELF32_ST_TYPE(symbol->st_info);
          
          if(type == STT_FUNC && symbol->st_value != 0) {
            INFO("  Function %s: %p - %p",
              string_table + symbol->st_name, 
              (void*)(uintptr_t)symbol->st_value, 
              (void*)((uintptr_t)symbol->st_value + symbol->st_size));
          }
          
          symbol_index++;
          symbol = (Symbol*)((uintptr_t)symbol + symbol_size);
        }
      } else {
        INFO(" Skipping section %s", shstr_table + sh_entry->sh_name);
      }
      
      // Advance to the next entry
      sh_index++;
      sh_entry = (SectionHeader*)((uintptr_t)sh_entry + sh_entry_size);
    }
  }
  
  void readElfFile(const char* filename, void* loaded) {
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
      if(header->e_ident[4] == ELFCLASS32) {
        // Handle an ELF32 file
        processElfFile<false, Elf32_Ehdr, Elf32_Shdr, Elf32_Sym>(filename, (Elf32_Ehdr*)header, loaded);
      } else if(header->e_ident[4] == ELFCLASS64) {
        // Handle an ELF64 file
        processElfFile<true, Elf64_Ehdr, Elf64_Shdr, Elf64_Sym>(filename, header, loaded);
      } else {
        WARNING("Unsupported ELF file class %d", header->e_ident[4]);
      }
    } else {
      WARNING("Not an elf file. Found magic bytes %#x %c%c%c",
        header->e_ident[0], header->e_ident[1], header->e_ident[2], header->e_ident[3]);
    }
    
    // Unmap and close
    REQUIRE(munmap(base, sb.st_size) != -1, "Failed to unmap file %s", filename);
    REQUIRE(close(fd) != -1, "Failed to close file %s", filename);
  }
  
  set<interval> _buildCodeMap() {
    set<interval> code;
    
    Dl_info dlinfo;
    
  	const PAPI_exe_info_t* info = PAPI_get_executable_info();
  	if(info) {
      // Find the load address for the main executable file
      dlinfo.dli_fbase = NULL;
      dladdr(info->address_info.text_start, &dlinfo);
      REQUIRE(dlinfo.dli_fbase != NULL, "Couldn't find base for file %s", info->fullname);
      
      // Process the ELF file for the main executable
      readElfFile(info->fullname, dlinfo.dli_fbase);
      
      // Add the main file's executable memory to the code map
      code.insert(interval(info->address_info.text_start, info->address_info.text_end));
  	}
    
    const PAPI_shlib_info_t* lib_info = PAPI_get_shared_lib_info();
    if(lib_info) {
      for(int i=0; i<lib_info->count; i++) {
        // Don't include PAPI or causal
        if(string(lib_info->map[i].name).find("libpapi") == string::npos &&
           string(lib_info->map[i].name).find("libcausal") == string::npos) {
             
          // Find the load address for this shared library
          dlinfo.dli_fbase = NULL;
          dladdr(lib_info->map[i].text_start, &dlinfo);
          REQUIRE(dlinfo.dli_fbase != NULL, "Couldn't file base for file %s", lib_info->map[i].name);
          
          // Process the ELF file for this library
          readElfFile(lib_info->map[i].name, dlinfo.dli_fbase);
          
          // Add this library's executable memory to the code map
          code.insert(interval(lib_info->map[i].text_start, lib_info->map[i].text_end));
        }
      }
    }
    
    INFO("All done!");
    exit(0);
    
    return code;
  }
  
  bool inRange(void* p) {
    static set<interval> code = _buildCodeMap();
    return code.find(p) != code.end();
  }
  
  function* getFunction(void* p) {
    static map<interval, function*> functions;
    
    if(!inRange(p))
      return NULL;
    
    // Look for an existing function object. If found, return it.
    map<interval, function*>::iterator iter = functions.find(p);
    if(iter != functions.end())
      return iter->second;
    
    function* f = function::get(p);
    if(f != NULL) {
      functions.insert(pair<const interval, function*>(f->getRange(), f));
    } else {
      functions.insert(pair<const interval, function*>(p, NULL));
    }
    
    return f;
  }
  
  basic_block* getBlock(void* p) {
    function* f = getFunction(p);
    if(f != NULL)
      return f->getBlock(p);
    else
      return NULL;
  }
}
