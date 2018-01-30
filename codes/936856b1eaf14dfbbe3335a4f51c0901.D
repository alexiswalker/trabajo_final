/**
 * This handles the API interfaces.
 *
 * Copyright: © 2015-2017, Dan Printzell
 * License: $(LINK2 https://www.mozilla.org/en-US/MPL/2.0/, Mozilla Public License Version 2.0)
 *  (See accompanying file LICENSE)
 * Authors: $(LINK2 https://vild.io/, Dan Printzell)
 */
module powerd.api;

import stl.address;

public import powerd.api.acpi;
public import powerd.api.cpu;

/// SemVer version format: Major.Minor.Patch
struct Version {
	// TODO: Change to a smaller unsigned interger container?
	size_t major; /// The major version
	size_t minor; /// The minor version
	size_t patch; /// The patch version
}

///
struct Module {
	char[] name; ///
	PhysMemoryRange memory; ///
}

///
struct MemoryMap {
	/// Copy of MultibootMemoryType
	enum Type {
		available = 1,
		reserved,
		acpiReclaimable,
		nvs,
		badRAM
	}

	PhysMemoryRange memory; ///
	Type type; ///
}

/// The PowerD information container
@safe struct PowerDAPI {
	import stl.vector : Vector;

	enum size_t magicValue = 0x3056_4472_6577_6F50UL; /// "PowerDV0" as size_t (backwards because of little-endian)
	size_t magic = magicValue; /// The magic
	// TODO: Sync with init32.S somehow
	Version version_ = Version(0, 0, 0); /// The PowerD version

	ubyte screenX;
	ubyte screenY;

	size_t ramAmount; ///

	Vector!Module modules; ///
	Vector!MemoryMap memoryMaps; ///

	PowerDACPI acpi; ///
	PowerDCPUs cpus; ///
}

ref PowerDAPI getPowerDAPI() @trusted {
	__gshared PowerDAPI powerDAPI;
	return powerDAPI;
}
