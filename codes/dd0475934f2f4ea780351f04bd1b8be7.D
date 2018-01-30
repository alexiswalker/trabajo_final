module data.multiboot;

import data.linker;
import data.textbuffer : scr = getBootTTY;
import io.log;
import data.address;

enum MultibootTagType {
	align_ = 8,
	end = 0,
	cmdLine,
	bootLoaderName,
	module_,
	basicMemInfo,
	bootDev,
	memoryMap,
	vbe,
	frameBuffer,
	elfSections,
	apm,
	efi32,
	efi64,
	smbios,
	acpiOld,
	acpiNew,
	network,
	efiMemoryMap,
	efibs
}

enum MultibootFramebufferType {
	indexed,
	rgd,
	egaText
}

enum MultibootMemoryType {
	available = 1,
	reserved,
	acpiReclaimable,
	nvs,
	badRAM
}

struct MultibootColor {
align(1):
	ubyte red;
	ubyte green;
	ubyte blue;
}

struct MultibootMemoryMap {
align(1):
	ulong address;
	ulong length;
	uint type;
	private uint _zero;
}

struct MultibootTag {
align(1):
	uint type;
	uint size;
}

struct MultibootTagString {
align(1):
	uint type;
	uint size;

	@property string string_() {
		return (VirtAddress(&size) + size.sizeof).ptr!char[0 .. size - MultibootTagString.sizeof - 1];
	}
}

struct MultibootTagModule {
align(1):
	uint type;
	uint size;
	uint modStart;
	uint modEnd;

	@property string string_() {
		return (VirtAddress(&modEnd) + modEnd.sizeof).ptr!char[0 .. size - MultibootTagModule.sizeof - 1];
	}
}

struct MultibootTagBasicMemInfo {
align(1):
	uint type;
	uint size;
	uint lower;
	uint upper;
}

struct MultibootTagBootDev {
align(1):
	uint type;
	uint size;
	uint biosDev;
	uint slice;
	uint part;
}

struct MultibootTagMemoryMap {
align(1):
	uint type;
	uint size;
	uint entrySize;
	uint entryVersion;
	MultibootMemoryMap entry;
}

struct MultibootTagOldACPI {
align(1):
	uint type;
	uint size;

	@property VirtAddress rsdp() {
		return VirtAddress(&size) + size.sizeof;
	}
}

struct MultibootTagNewACPI {
align(1):
	uint type;
	uint size;

	@property VirtAddress rsdp() {
		return VirtAddress(&size) + size.sizeof;
	}
}

struct MultibootTagFramebufferCommon {
align(1):
	uint type;
	uint size;

	ulong address;
	uint pitch;
	uint width;
	uint geight;
	ubyte bpp;
	ubyte framebufferType;
	private ushort _reserved;
}

struct MultibootTagFramebuffer {
align(1):
	MultibootTagFramebufferCommon common;

	union {
		struct {
			ushort paletteNumColors;
			MultibootColor palette;
		}

		struct {
			ubyte redFieldPos;
			ubyte redMaskSize;
			ubyte greenFieldPos;
			ubyte greenMaskSize;
			ubyte blueFieldPos;
			ubyte blueMaskSize;
		}
	}
}

struct Multiboot {
	private enum {
		headerMagic = 0xE85250D6,
		bootloaderMagic = 0x36D76289
	}

	__gshared {
		MultibootTagModule*[256] modules;
		MultibootMemoryMap*[256] memoryMap;
		int modulesCount;
		int memoryMapCount;
		ulong memorySize;

		VirtAddress acpiRSDPV10;
		VirtAddress acpiRSDPV20;

		string[256] cmdKey;
		string[256] cmdValue;
		size_t cmdLength;
	}

	static void parseHeader(uint magic, ulong info) {
		if (magic != bootloaderMagic) {
			scr.writeln("Error: Bad multiboot 2 magic: %d", magic);
			while (true) {
			}
		}

		if (info & 7) {
			scr.writeln("Error: Unaligned MBI");
			while (true) {
			}
		}

		MultibootTag* mbt = cast(MultibootTag*)(info + Linker.kernelStart + 8);
		for (; mbt.type != MultibootTagType.end; mbt = cast(MultibootTag*)(cast(ulong)mbt + ((mbt.size + 7UL) & ~7UL))) {
			switch (mbt.type) {
			case MultibootTagType.cmdLine:
				auto tmp = cast(MultibootTagString*)mbt;
				string str = tmp.string_;
				Log.debug_("CmdLine: ", str);
				if (!str.length)
					break;

				size_t start;
				size_t divider;
				size_t cur;
				while (cur < str.length) {
					if (str[cur] == ' ') {
						cmdKey[cmdLength] = (start == divider) ? null : str[start .. (start < divider) ? divider : cur];
						cmdValue[cmdLength] = (start > divider) ? null : str[divider + 1 .. cur];
						Log.debug_("\tKey: '", cmdKey[cmdLength], "' Value: '", cmdValue[cmdLength], "'");
						cmdLength++;
						start = cur + 1;
					} else if (str[cur] == '=')
						divider = cur;
					cur++;
				}
				cmdKey[cmdLength] = (start == divider) ? null : str[start .. (start < divider) ? divider : cur];
				cmdValue[cmdLength] = (start > divider) ? null : str[divider + 1 .. cur];
				Log.debug_("\tKey: '", cmdKey[cmdLength], "' Value: '", cmdValue[cmdLength], "'");
				cmdLength++;
				break;

			case MultibootTagType.bootLoaderName:
				break;

			case MultibootTagType.module_:
				auto tmp = cast(MultibootTagModule*)mbt;

				modules[modulesCount++] = tmp;

				Log.info("Name: Module (", cast(void*)tmp, "), Start: ", cast(void*)tmp.modStart, ", End: ",
						cast(void*)tmp.modEnd, ", Name: '", tmp.string_, "'");
				break;

			case MultibootTagType.basicMemInfo:
				auto tmp = cast(MultibootTagBasicMemInfo*)mbt;
				memorySize = tmp.lower + tmp.upper;
				break;

			case MultibootTagType.bootDev:
				break;

			case MultibootTagType.memoryMap:
				for (auto tmp = &(cast(MultibootTagMemoryMap*)mbt).entry; cast(void*)tmp < (cast(void*)mbt + mbt.size); tmp = cast(
						MultibootMemoryMap*)(cast(ulong)tmp + (cast(MultibootTagMemoryMap*)mbt).entrySize)) {
					memoryMap[memoryMapCount++] = tmp;
					Log.info("BaseAddr: ", cast(void*)tmp.address, ", Length: ", cast(void*)tmp.length, ", Type: ", tmp.type);
				}
				break;

			case MultibootTagType.vbe:
				break;

			case MultibootTagType.frameBuffer:
				break;

			case MultibootTagType.elfSections:
				break;

			case MultibootTagType.apm:
				break;

			case MultibootTagType.efi32:
				break;

			case MultibootTagType.efi64:
				break;

			case MultibootTagType.smbios:
				break;

			case MultibootTagType.acpiOld:
				auto tmp = cast(MultibootTagOldACPI*)mbt;
				Log.info("acpiOld: ", tmp.rsdp, " size: ", tmp.size - MultibootTagOldACPI.sizeof);
				acpiRSDPV10 = tmp.rsdp;
				break;

			case MultibootTagType.acpiNew:
				auto tmp = cast(MultibootTagNewACPI*)mbt;
				Log.info("acpiNew: ", tmp.rsdp, " size: ", tmp.size - MultibootTagNewACPI.sizeof);
				acpiRSDPV20 = tmp.rsdp;
				break;

			case MultibootTagType.network:
				break;

			case MultibootTagType.efiMemoryMap:
				break;

			case MultibootTagType.efibs:
				break;

			default:
				scr.writeln("Multiboot2 Error tag type: ", mbt.type);
				break;
			}
		}
	}

	import arch.paging : _makeAddress;

	private __gshared VirtAddress _moduleMapping = _makeAddress(510, 0, 2, 0);

	static VirtAddress[2] getModule(string name) {
		import arch.paging : kernelHWPaging;
		import memory.vmm : VMPageFlags;

		foreach (mod; modules[0 .. modulesCount]) {
			if (mod.string_ != name)
				continue;

			auto startPage = _moduleMapping;

			auto vAddr = startPage;
			auto pAddr = mod.modStart.PhysAddress & ~0xFFF;
			auto pAddrEnd = mod.modEnd.PhysAddress;

			size_t pagesUsed;
			do {
				pagesUsed++;
				kernelHWPaging.map(vAddr, pAddr, VMPageFlags.present | VMPageFlags.writable /* Is needed ? */ );

				vAddr += 0x1000;
				pAddr += 0x1000;
			}
			while (pAddr < pAddrEnd);

			_moduleMapping += 0x1000 * pagesUsed;

			VirtAddress startMod = startPage + (mod.modStart & 0xFFF);
			VirtAddress endMod = startMod + (mod.modEnd - mod.modStart);
			return [startMod, endMod];

		}
		return [VirtAddress(), VirtAddress()];
	}

	static string* getArgument(string key) {
		foreach (idx, k; cmdKey[0 .. cmdLength])
			if (k == key)
				return &cmdValue[idx];
		return null;
	}
}
