/***********************************************************************************************************************************
Command and Option Parse
***********************************************************************************************************************************/
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>

#include "common/error.h"
#include "common/ini.h"
#include "common/memContext.h"
#include "config/parse.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Parse option flags
***********************************************************************************************************************************/
// Offset the option values so they don't conflict with getopt_long return codes
#define PARSE_OPTION_FLAG                                           (1 << 31)

// Add a flag for negation rather than checking "-no-"
#define PARSE_NEGATE_FLAG                                           (1 << 30)

// Mask to exclude all flags and get at the actual option id (only 12 bits allowed for option id, the rest reserved for flags)
#define PARSE_OPTION_MASK                                           0xFFF

/***********************************************************************************************************************************
Include automatically generated data structure for getopt_long()
***********************************************************************************************************************************/
#include "parse.auto.c"

/***********************************************************************************************************************************
Struct to hold options parsed from the command line
***********************************************************************************************************************************/
typedef struct ParseOption
{
    bool found:1;                                                   // Was the option found on the command line?
    bool negate:1;                                                  // Was the option negated on the command line?
    unsigned int source:2;                                          // Where was to option found?
    StringList *valueList;                                          // List of values found
} ParseOption;

/***********************************************************************************************************************************
Parse the command-line arguments and config file to produce final config data
***********************************************************************************************************************************/
void
configParse(int argListSize, const char *argList[])
{
    // Initialize configuration
    cfgInit();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set the exe
        cfgExeSet(strNew(argList[0]));

        // Phase 1: parse command line parameters
        // -------------------------------------------------------------------------------------------------------------------------
        int option;                                                     // Code returned by getopt_long
        int optionListIdx;                                              // Index of option is list (if an option was returned)
        ConfigOption optionId;                                          // Option id extracted from option var
        bool negate;                                                    // Option is being negated
        bool argFound = false;                                          // Track args found to decide on error or help at the end
        StringList *commandParamList = NULL;                            // List of command  parameters

        // Reset optind to 1 in case getopt_long has been called before
        optind = 1;

        // Don't error automatically on unknown options - they will be processed in the loop below
        opterr = false;

        // List of parsed options
        ParseOption parseOptionList[CFG_OPTION_TOTAL];
        memset(&parseOptionList, 0, sizeof(parseOptionList));

        // Only the first non-option parameter should be treated as a command so track if the command has been set
        bool commandSet = false;

        while ((option = getopt_long(argListSize, (char **)argList, "-:", optionList, &optionListIdx)) != -1)
        {
            switch (option)
            {
                // Parse arguments that are not options, i.e. commands and parameters passed to commands
                case 1:
                {
                    // The first argument should be the command
                    if (!commandSet)
                    {
                        // Try getting the command from the valid command list
                        TRY_BEGIN()
                        {
                            cfgCommandSet(cfgCommandId(argList[optind - 1]));
                        }
                        // Assert error means the command does not exist, which is correct for all usages but this one (since we
                        // don't have any control over what the user passes), so modify the error code and message.
                        CATCH(AssertError)
                        {
                            THROW(CommandInvalidError, "invalid command '%s'", argList[optind - 1]);
                        }
                        TRY_END();

                        if (cfgCommand() == cfgCmdHelp)
                            cfgCommandHelpSet(true);
                        else
                            commandSet = true;
                    }
                    // Additional arguments are command arguments
                    else
                    {
                        if (commandParamList == NULL)
                            commandParamList = strLstNew();

                        strLstAdd(commandParamList, strNew(argList[optind - 1]));
                    }

                    break;
                }

                // If the option is unknown then error
                case '?':
                    THROW(OptionInvalidError, "invalid option '%s'", argList[optind - 1]);
                    break;                                              // {uncoverable - case statement does not return}

                // If the option is missing an argument then error
                case ':':
                    THROW(OptionInvalidError, "option '%s' requires argument", argList[optind - 1]);
                    break;                                              // {uncoverable - case statement does not return}

                // Parse valid option
                default:
                    // Get option id and flags from the option code
                    optionId = option & PARSE_OPTION_MASK;
                    negate = option & PARSE_NEGATE_FLAG;

                    // Make sure the option id is valid
                    assert(optionId < CFG_OPTION_TOTAL);

                    // If the the option has not been found yet then set it
                    if (!parseOptionList[optionId].found)
                    {
                        parseOptionList[optionId].found = true;
                        parseOptionList[optionId].negate = negate;
                        parseOptionList[optionId].source = cfgSourceParam;

                        // Only set the argument if the option requires one
                        if (optionList[optionListIdx].has_arg == required_argument)
                            parseOptionList[optionId].valueList = strLstAdd(strLstNew(), strNew(optarg));
                    }
                    else
                    {
                        // Make sure option is not negated more than once.  It probably wouldn't hurt anything to accept this case
                        // but there's no point in allowing the user to be sloppy.
                        if (parseOptionList[optionId].negate && negate)
                            THROW(OptionInvalidError, "option '%s' is negated multiple times", cfgOptionName(optionId));

                        // Don't allow an option to be both set and negated
                        if (parseOptionList[optionId].negate != negate)
                            THROW(OptionInvalidError, "option '%s' cannot be set and negated", cfgOptionName(optionId));

                        // Error if this option does not allow multiple arguments
                        if (!(cfgDefOptionType(cfgOptionDefIdFromId(optionId)) == cfgDefOptTypeHash ||
                              cfgDefOptionType(cfgOptionDefIdFromId(optionId)) == cfgDefOptTypeList))
                        {
                            THROW(OptionInvalidError, "option '%s' cannot have multiple arguments", cfgOptionName(optionId));
                        }

                        // Add the argument
                        strLstAdd(parseOptionList[optionId].valueList, strNew(optarg));
                    }

                    break;
            }

            // Arg has been found
            argFound = true;
        }

        // Handle command not found
        if (!commandSet && !cfgCommandHelp())
        {
            // If there are args then error
            if (argFound)
                THROW(CommandRequiredError, "no command found");

            // Otherwise set the comand to help
            cfgCommandHelpSet(true);
        }

        // Set command params
        if (commandParamList != NULL)
            cfgCommandParamSet(commandParamList);

        // Parse options from config file unless --no-config passed
        if (cfgCommand() != cfgCmdNone &&
            cfgCommand() != cfgCmdVersion &&
            cfgCommand() != cfgCmdHelp)
        {
            // Get the command definition id
            ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(cfgCommand());

            // Phase 2: parse config file
            // ---------------------------------------------------------------------------------------------------------------------
            if (!parseOptionList[cfgOptConfig].negate)
            {
                // Get the config file name from the command-line if it exists else default
                const String *configFile = NULL;

                if (parseOptionList[cfgOptConfig].found)
                    configFile = strLstGet(parseOptionList[cfgOptConfig].valueList, 0);
                else
                    configFile = strNew(cfgDefOptionDefault(commandDefId, cfgOptionDefIdFromId(cfgOptConfig)));

                // Load the ini file
                Buffer *buffer = storageGet(storageLocal(), configFile, !parseOptionList[cfgOptConfig].found);

                // Load the config file if it was found
                if (buffer != NULL)
                {
                    // Parse the ini file
                    Ini *config = iniNew();
                    iniParse(config, strNewBuf(buffer));

                    // Get the stanza name
                    String *stanza = NULL;

                    if (parseOptionList[cfgOptStanza].found)
                        stanza = strLstGet(parseOptionList[cfgOptStanza].valueList, 0);

                    // Build list of sections to search for options
                    StringList *sectionList = strLstNew();

                    if (stanza != NULL)
                    {
                        strLstAdd(sectionList, strNewFmt("%s:%s", strPtr(stanza), cfgCommandName(cfgCommand())));
                        strLstAdd(sectionList, stanza);
                    }

                    strLstAdd(sectionList, strNewFmt(CFGDEF_SECTION_GLOBAL ":%s", cfgCommandName(cfgCommand())));
                    strLstAdd(sectionList, strNew(CFGDEF_SECTION_GLOBAL));

                    // Loop through sections to search for options
                    for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
                    {
                        String *section = strLstGet(sectionList, sectionIdx);
                        StringList *keyList = iniSectionKeyList(config, section);

                        // Loop through keys to search for options
                        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                        {
                            String *key = strLstGet(keyList, keyIdx);

                            // Find the optionName in the main list
                            unsigned int optionIdx = 0;

                            while (optionList[optionIdx].name != NULL)
                            {
                                if (strcmp(strPtr(key), optionList[optionIdx].name) == 0)
                                    break;

                                optionIdx++;
                            }

                            // Warn if the option not found
                            if (optionList[optionIdx].name == NULL)
                            {
                                /// ??? Put warning here once there is a logging system
                                continue;
                            }
                            // Warn if negate option found in config
                            else if (optionList[optionIdx].val & PARSE_NEGATE_FLAG)
                            {
                                /// ??? Put warning here once there is a logging system
                                continue;
                            }

                            optionId = optionList[optionIdx].val & PARSE_OPTION_MASK;
                            ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                            /// Warn if this option should be command-line only
                            if (cfgDefOptionSection(optionDefId) == cfgDefSectionCommandLine)
                            {
                                /// ??? Put warning here once there is a logging system
                                continue;
                            }

                            // Continue if this option has already been found
                            if (parseOptionList[optionId].found)
                                continue;

                            // Continue if the option is not valid for this command
                            if (!cfgDefOptionValid(commandDefId, optionDefId))
                            {
                                // Warn if it is in a command section
                                if (sectionIdx % 2 == 0)
                                {
                                    // ??? Put warning here once there is a logging system (and remove continue and braces)
                                    continue;
                                }

                                continue;
                            }

                            // Only get the option if it is valid for this section
                            if ((stanza != NULL && sectionIdx < 2) || cfgDefOptionSection(optionDefId) == cfgDefSectionGlobal)
                            {
                                const Variant *value = iniGetDefault(config, section, key, NULL);

                                if (varType(value) == varTypeString && strSize(varStr(value)) == 0)
                                    THROW(OptionInvalidValueError, "section '%s', key '%s' must have a value", strPtr(section),
                                    strPtr(key));

                                parseOptionList[optionId].found = true;
                                parseOptionList[optionId].source = cfgSourceConfig;

                                // Convert boolean to string
                                if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                                {
                                    if (strcasecmp(strPtr(varStr(value)), "n") == 0)
                                        parseOptionList[optionId].negate = true;
                                    else if (strcasecmp(strPtr(varStr(value)), "y") != 0)
                                        THROW(OptionInvalidError, "boolean option '%s' must be 'y' or 'n'", strPtr(key));
                                }
                                // Else add the string value
                                else if (varType(value) == varTypeString)
                                {
                                    parseOptionList[optionId].valueList = strLstNew();
                                    strLstAdd(parseOptionList[optionId].valueList, varStr(value));
                                }
                                // Else add the string list
                                else
                                    parseOptionList[optionId].valueList = strLstNewVarLst(varVarLst(value));
                            }
                        }
                    }
                }
            }

            // Phase 3: validate option definitions and load into configuration
            // ---------------------------------------------------------------------------------------------------------------------
            bool allResolved;
            bool optionResolved[CFG_OPTION_TOTAL] = {false};

            do
            {
                // Assume that all dependencies will be resolved in this loop.  This probably won't be true the first few times, but
                // eventually it will be or the do loop would never exit.
                allResolved = true;

                // Loop through all options
                for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
                {
                    // Get the option data parsed from the command-line
                    ParseOption *parseOption = &parseOptionList[optionId];

                    // Get the option definition id -- will be used to look up option rules
                    ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);
                    ConfigDefineOptionType optionDefType = cfgDefOptionType(optionDefId);

                    // Skip this option if it has already been resolved
                    if (optionResolved[optionId])
                        continue;

                    // Error if the option is not valid for this command
                    if (parseOption->found && !cfgDefOptionValid(commandDefId, optionDefId))
                    {
                        THROW(
                            OptionInvalidError, "option '%s' not valid for command '%s'", cfgOptionName(optionId),
                            cfgCommandName(cfgCommand()));
                    }

                    // Is the option valid for this command?  If not, mark it as resolved since there is nothing more to do.
                    cfgOptionValidSet(optionId, cfgDefOptionValid(commandDefId, optionDefId));

                    if (!cfgOptionValid(optionId))
                    {
                        optionResolved[optionId] = true;
                        continue;
                    }

                    // Is the value set for this option?
                    bool optionSet = parseOption->found && (optionDefType == cfgDefOptTypeBoolean || !parseOption->negate);

                    // Set negate flag
                    cfgOptionNegateSet(optionId, parseOption->negate);

                    // Check option dependencies
                    bool dependResolved = true;

                    if (cfgDefOptionDepend(commandDefId, optionDefId))
                    {
                        ConfigOption dependOptionId =
                            cfgOptionIdFromDefId(cfgDefOptionDependOption(commandDefId, optionDefId), cfgOptionIndex(optionId));
                        ConfigDefineOption dependOptionDefId = cfgOptionDefIdFromId(dependOptionId);
                        ConfigDefineOptionType dependOptionDefType = cfgDefOptionType(dependOptionDefId);

                        // Make sure the depend option has been resolved, otherwise skip this option for now
                        if (!optionResolved[dependOptionId])
                        {
                            allResolved = false;
                            continue;
                        }

                        // Get the depend option value
                        const Variant *dependValue = cfgOption(dependOptionId);

                        if (dependValue != NULL)
                        {
                            if (dependOptionDefType == cfgDefOptTypeBoolean)
                            {
                                if (cfgOptionBool(dependOptionId))
                                    dependValue = varNewStrZ("1");
                                else
                                    dependValue = varNewStrZ("0");
                            }
                        }

                        // Can't resolve if the depend option value is null
                        if (dependValue == NULL)
                        {
                            dependResolved = false;

                            // if (optionSet)
                            if (optionSet && parseOption->source == cfgSourceParam)
                            {
                                THROW(
                                    OptionInvalidError, "option '%s' not valid without option '%s'", cfgOptionName(optionId),
                                    cfgOptionName(dependOptionId));
                            }
                        }
                        // If a depend list exists, make sure the value is in the list
                        else if (cfgDefOptionDependValueTotal(commandDefId, optionDefId) > 0)
                        {
                            dependResolved = cfgDefOptionDependValueValid(commandDefId, optionDefId, strPtr(varStr(dependValue)));

                            // If not depend not resolved and option value is set then error
                            if (!dependResolved && optionSet && parseOption->source == cfgSourceParam)
                            {
                                // Get the depend option name
                                String *dependOptionName = strNew(cfgOptionName(dependOptionId));

                                // Build the list of possible depend values
                                StringList *dependValueList = strLstNew();

                                for (int listIdx = 0; listIdx < cfgDefOptionDependValueTotal(commandDefId, optionDefId); listIdx++)
                                {
                                    const char *dependValue = cfgDefOptionDependValue(commandDefId, optionDefId, listIdx);

                                    // Build list based on depend option type
                                    switch (dependOptionDefType)
                                    {
                                        // Boolean outputs depend option name as no-* when false
                                        case cfgDefOptTypeBoolean:
                                        {
                                            if (strcmp(dependValue, "0") == 0)
                                                dependOptionName = strNewFmt("no-%s", cfgOptionName(dependOptionId));

                                            break;
                                        }

                                        // String is output with quotes
                                        case cfgDefOptTypeString:
                                        {
                                            strLstAdd(dependValueList, strNewFmt("'%s'", dependValue));
                                            break;
                                        }

                                        // Other types are output plain
                                        default:
                                        {
                                            strLstAddZ(dependValueList, dependValue);   // {uncovered - no depends of other types}
                                            break;                                      // {+uncovered}
                                        }
                                    }
                                }

                                // Build the error string
                                String *error = strNew("option '%s' not valid without option '%s'");

                                if (strLstSize(dependValueList) == 1)
                                    strCat(error, " = %s");
                                else if (strLstSize(dependValueList) > 1)
                                    strCat(error, " in (%s)");

                                // Throw the error
                                THROW(
                                    OptionInvalidError, strPtr(error), cfgOptionName(optionId), strPtr(dependOptionName),
                                    strPtr(strLstJoin(dependValueList, ", ")));
                            }
                        }
                    }

                    // Is the option defined?
                    if (optionSet && dependResolved)
                    {
                        if (optionDefType == cfgDefOptTypeBoolean)
                        {
                            cfgOptionSet(optionId, parseOption->source, varNewBool(!parseOption->negate));
                        }
                        else if (optionDefType == cfgDefOptTypeHash)
                        {
                            Variant *value = varNewKv();
                            KeyValue *keyValue = varKv(value);

                            for (unsigned int listIdx = 0; listIdx < strLstSize(parseOption->valueList); listIdx++)
                            {
                                const char *pair = strPtr(strLstGet(parseOption->valueList, listIdx));
                                const char *equal = strchr(pair, '=');

                                if (equal == NULL)
                                {
                                    THROW(
                                        OptionInvalidError, "key/value '%s' not valid for '%s' option",
                                        strPtr(strLstGet(parseOption->valueList, listIdx)), cfgOptionName(optionId));
                                }

                                kvPut(keyValue, varNewStr(strNewN(pair, equal - pair)), varNewStr(strNew(equal + 1)));
                            }

                            cfgOptionSet(optionId, parseOption->source, value);
                        }
                        else if (optionDefType == cfgDefOptTypeList)
                        {
                            cfgOptionSet(optionId, parseOption->source, varNewVarLst(varLstNewStrLst(parseOption->valueList)));
                        }
                        else
                        {
                            String *value = strLstGet(parseOption->valueList, 0);

                            // If the option has an allow list then check it
                            if (cfgDefOptionAllowList(commandDefId, optionDefId) &&
                                !cfgDefOptionAllowListValueValid(commandDefId, optionDefId, strPtr(value)))
                            {
                                THROW(
                                    OptionInvalidValueError, "'%s' is not valid for '%s' option", strPtr(value),
                                    cfgOptionName(optionId));
                            }

                            // If a numeric type check that the value is valid
                            if (optionDefType == cfgDefOptTypeInteger || optionDefType == cfgDefOptTypeFloat)
                            {
                                double valueDbl = 0;

                                // Check that the value can be converted
                                TRY_BEGIN()
                                {
                                    if (optionDefType == cfgDefOptTypeInteger)
                                        valueDbl = varIntForce(varNewStr(value));
                                    else
                                        valueDbl = varDblForce(varNewStr(value));
                                }
                                CATCH_ANY()
                                {
                                    THROW(
                                        OptionInvalidValueError, "'%s' is not valid for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }
                                TRY_END();

                                // Check value range
                                if (cfgDefOptionAllowRange(commandDefId, optionDefId) &&
                                    (valueDbl < cfgDefOptionAllowRangeMin(commandDefId, optionDefId) ||
                                     valueDbl > cfgDefOptionAllowRangeMax(commandDefId, optionDefId)))
                                {
                                    THROW(
                                        OptionInvalidValueError, "'%s' is not valid for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }
                            }

                            cfgOptionSet(optionId, parseOption->source, varNewStr(value));
                        }
                    }
                    else if (dependResolved && parseOption->negate)
                        cfgOptionSet(optionId, parseOption->source, NULL);
                    // Else try to set a default
                    else if (dependResolved)
                    {
                        // Get the default value for this option
                        const char *value = cfgDefOptionDefault(commandDefId, optionDefId);

                        if (value != NULL)
                            cfgOptionSet(optionId, cfgSourceDefault, varNewStrZ(value));
                        else if (cfgOptionIndex(optionId) == 0 && cfgDefOptionRequired(commandDefId, optionDefId) &&
                                 !cfgCommandHelp())
                        {
                            const char *hint = "";

                            if (cfgDefOptionSection(optionDefId) == cfgDefSectionStanza)
                                hint = "\nHINT: does this stanza exist?";

                            THROW(
                                OptionRequiredError, "%s command requires option: %s%s", cfgCommandName(cfgCommand()),
                                cfgOptionName(optionId), hint);
                        }
                    }

                    // Option is now resolved
                    optionResolved[optionId] = true;
                }
            }
            while (!allResolved);
        }
    }
    MEM_CONTEXT_TEMP_END();
}
