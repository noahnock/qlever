// Copyright 2023, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Andre Schlegel (March of 2023, schlegea@informatik.uni-freiburg.de)

#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <gtest/gtest.h>

#include <any>
#include <concepts>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <variant>
#include <vector>

#include "util/ConfigManager/ConfigExceptions.h"
#include "util/ConfigManager/ConfigOption.h"
#include "util/ConfigManager/ConfigOptionProxy.h"
#include "util/ConfigManager/ConfigUtil.h"
#include "util/ConfigManager/generated/ConfigShorthandLexer.h"
#include "util/Exception.h"
#include "util/Forward.h"
#include "util/HashMap.h"
#include "util/StringUtils.h"
#include "util/TypeTraits.h"
#include "util/json.h"

namespace ad_utility {

/*
A validator function is any invocable object, who takes the given parameter
types, in the same order and transformed to `const type&`, and returns a
bool.

This concept is used for `ConfigManager::addValidator`.
*/
template <typename Func, typename... ParameterTypes>
concept Validator =
    std::regular_invocable<Func, const ParameterTypes&...> &&
    std::same_as<std::invoke_result_t<Func, const ParameterTypes&...>, bool>;

// Simple struct, that holds an error message. For use as the return type of
// invocable object, that fullfill `ExceptionValidator`.
struct ErrorMessage {
 private:
  std::string message_;

 public:
  // Constructor.
  explicit ErrorMessage(std::string message) : message_{std::move(message)} {}

  // Getter for the message.
  const std::string& getMessage() const;
};

/*
An exception validator function is any invocable object, who takes the given
parameter types, in the same order and transformed to `const type&`, and returns
an instance of `std::optional<ErrorMessage>`.

This concept is used for `ConfigManager::addValidator`.
*/
template <typename Func, typename... ParameterTypes>
concept ExceptionValidator =
    std::regular_invocable<Func, const ParameterTypes&...> &&
    std::same_as<std::invoke_result_t<Func, const ParameterTypes&...>,
                 std::optional<ErrorMessage>>;

/*
Manages a bunch of `ConfigOption`s.
*/
class ConfigManager {
  /*
  The added configuration options. Configuration managers are used by the user
  to describe a json object literal more explicitly.

  A configuration option tends to be placed like a key value pair in a json
  object. For example: `{"object 1" : [{"object 2" : { "The configuration option
  identifier" : "Configuration information"} }]}`

  The string key describes their location in the json object literal, by
  representing a json pointer in string form.
  */
  using HashMapEntry =
      std::unique_ptr<std::variant<ConfigOption, ConfigManager>>;
  ad_utility::HashMap<std::string, HashMapEntry> configurationOptions_;

  /*
  List of the added validators. Whenever the values of the options are set,
  they are called afterwards with `verifyWithValidators` to make sure, that
  the new values are valid.
  If the new value isn't valid, it throws an exception.
  */
  std::vector<std::function<void(void)>> validators_;

 public:
  /*
  @brief Creates and adds a new configuration option without a default value.
  This configuration option must always be set at runtime.

  @tparam OptionType The type of value, the configuration option can hold.

  @param pathToOption Describes a path in json, that points to the value held by
  the configuration option. The last key in the vector is the name of the
  configuration option.
  @param optionDescription A description for the configuration option.
  @param variableToPutValueOfTheOptionIn The value held by the configuration
  option will be copied into this variable, whenever the value in the
  configuration option changes.

  @return A reference to the newly created configuration option. This reference
  will stay valid, even after adding more options.
  */
  template <typename OptionType>
  requires ad_utility::isTypeContainedIn<OptionType,
                                         ConfigOption::AvailableTypes>
  ConstConfigOptionProxy<OptionType> addOption(
      const std::vector<std::string>& pathToOption,
      std::string_view optionDescription,
      OptionType* variableToPutValueOfTheOptionIn) {
    return addOptionImpl(pathToOption, optionDescription,
                         variableToPutValueOfTheOptionIn,
                         std::optional<OptionType>(std::nullopt));
  }

  /*
  @brief Creates and adds a new configuration option with a default value.
  Setting this option at runtime is optional and not required.

  @tparam OptionType The type of value, the configuration option can hold.

  @param pathToOption Describes a path in json, that points to the value held by
  the configuration option. The last key in the vector is the name of the
  configuration option.
  @param optionDescription A description for the configuration option.
  @param variableToPutValueOfTheOptionIn The value held by the configuration
  option will be copied into this variable, whenever the value in the
  configuration option changes.
  @param defaultValue A default value for the configuration option.

  @return A reference to the newly created configuration option. This reference
  will stay valid, even after adding more options.
  */
  template <typename OptionType,
            std::same_as<OptionType> DefaultValueType = OptionType>
  requires ad_utility::isTypeContainedIn<OptionType,
                                         ConfigOption::AvailableTypes>
  ConstConfigOptionProxy<OptionType> addOption(
      const std::vector<std::string>& pathToOption,
      std::string_view optionDescription,
      OptionType* variableToPutValueOfTheOptionIn,
      DefaultValueType defaultValue) {
    return addOptionImpl(pathToOption, optionDescription,
                         variableToPutValueOfTheOptionIn,
                         std::optional<OptionType>(std::move(defaultValue)));
  }

  /*
  @brief Creates and adds a new configuration option, just like in the other
  `addOption`. But instead of a `pathToOption`, there is only an
  `optionName`, which describes a path only made out of this single string.

  @return A reference to the newly created configuration option. This reference
  will stay valid, even after adding more options.
  */
  template <typename OptionType>
  requires ad_utility::isTypeContainedIn<OptionType,
                                         ConfigOption::AvailableTypes>
  ConstConfigOptionProxy<OptionType> addOption(
      std::string optionName, std::string_view optionDescription,
      OptionType* variableToPutValueOfTheOptionIn) {
    return addOption<OptionType>(
        std::vector<std::string>{std::move(optionName)}, optionDescription,
        variableToPutValueOfTheOptionIn);
  }

  /*
  @brief Creates and adds a new configuration option, just like in the other
  `addOption`. But instead of a `pathToOption`, there is only an
  `optionName`, which describes a path only made out of this single string.

  @return A reference to the newly created configuration option. This reference
  will stay valid, even after adding more options.
  */
  template <typename OptionType,
            std::same_as<OptionType> DefaultValueType = OptionType>
  requires ad_utility::isTypeContainedIn<OptionType,
                                         ConfigOption::AvailableTypes>
  ConstConfigOptionProxy<OptionType> addOption(
      std::string optionName, std::string_view optionDescription,
      OptionType* variableToPutValueOfTheOptionIn,
      DefaultValueType defaultValue) {
    return addOption<OptionType>(
        std::vector<std::string>{std::move(optionName)}, optionDescription,
        variableToPutValueOfTheOptionIn, std::move(defaultValue));
  }

  /*
  @brief Creates and adds a new configuration manager with a prefix path for
  it's internally held configuration options and managers.

  @param pathToOption Describes a path in json, which will be a prefix to all
  the other paths held in the newly created ConfigManager.

  @return A reference to the newly created configuration manager. This reference
  will stay valid, even after adding more options.
  */
  ConfigManager& addSubManager(const std::vector<std::string>& path);

  /*
  @brief Sets the configuration options based on the given json.

  @param j There will be an exception thrown, if:
  - `j` doesn't contain values for all configuration options, that must be set
  at runtime.
  - Same, if there are values for configuration options, that do not exist, or
  are not contained in this manager, or its sub mangangers.
  - `j` is anything but a json object literal.
  - Any of the added validators return false.
  */
  void parseConfig(const nlohmann::json& j);

  /*
  @brief Parses the given short hand and returns it as a json object,
  that contains all the described configuration data.

  @param shortHandString The language of the short hand is defined in
  `generated/ConfigShorthand.g4`. The short hand is a format similar to JSON
  that is more suitable to be set directly from the command line
  */
  static nlohmann::json parseShortHand(const std::string& shortHandString);

  /*
  @brief Returns a string containing a json configuration and the
  string representations of all added configuration options. Followed by a list
  of all the configuration options, that kept their default values.

  @param printCurrentJsonConfiguration If true, the current values of the
  configuration options will be used for printing the json configuration. If
  false, an example json configuration will be printed, that uses the default
  values of the configuration options, and, if an option doesn't have a default
  value, a dummy value.
  */
  std::string printConfigurationDoc(bool printCurrentJsonConfiguration) const;

  /*
  @brief Add a function to the configuration manager for verifying, that the
  values of the given configuration options are valid. Will always be called
  after parsing.

  @tparam ValidatorParameterTypes The types of the values in the
  `configOptionsToBeChecked`. Can be left up to type deduction.

  @param validatorFunction Checks, if the values of the given configuration
  options are valid. Should return true, if they are valid.
  @param errorMessage A `std::runtime_error` with this as an error message will
  get thrown, if the `validatorFunction` returns false.
  @param configOptionsToBeChecked Proxies for the configuration options, whos
  values will be passed to the validator function as function arguments. Will
  keep the same order.
  */
  template <typename... ValidatorParameterTypes>
  void addValidator(
      Validator<ValidatorParameterTypes...> auto validatorFunction,
      std::string errorMessage,
      ConstConfigOptionProxy<
          ValidatorParameterTypes>... configOptionsToBeChecked)
      requires(sizeof...(configOptionsToBeChecked) > 0) {
    addValidatorImpl(
        "addValidator",
        []<typename T>(ConstConfigOptionProxy<T> opt) {
          return opt.getConfigOption().template getValue<std::decay_t<T>>();
        },
        transformValidatorIntoExceptionValidator<ValidatorParameterTypes...>(
            validatorFunction, std::move(errorMessage)),
        configOptionsToBeChecked...);
  }

  /*
  @brief Add a function to the configuration manager for verifying, that the
  values of the given configuration options are valid. Will always be called
  after parsing.

  @tparam ValidatorParameterTypes The types of the values in the
  `configOptionsToBeChecked`. Can be left up to type deduction.

  @param exceptionValidatorFunction Checks, if the values of the given
  configuration options are valid. Return an empty instance of
  `std::optional<ErrorMessage>` if valid. Otherwise, the `ErrorMessage` should
  contain the reason, why the values are none valid.
  @param configOptionsToBeChecked Proxies for the configuration options, the
  values of which will be passed to the exception validator function as function
  arguments. Will keep the same order.
  */
  template <typename... ExceptionValidatorParameterTypes>
  void addValidator(
      ExceptionValidator<ExceptionValidatorParameterTypes...> auto
          exceptionValidatorFunction,
      ConstConfigOptionProxy<
          ExceptionValidatorParameterTypes>... configOptionsToBeChecked)
      requires(sizeof...(configOptionsToBeChecked) > 0) {
    addValidatorImpl(
        "addValidator",
        []<typename T>(ConstConfigOptionProxy<T> opt) {
          return opt.getConfigOption().template getValue<std::decay_t<T>>();
        },
        exceptionValidatorFunction, configOptionsToBeChecked...);
  }

  /*
  @brief Add a function to the configuration manager for verifying, that the
  given configuration options are valid. Will always be called after parsing.

  @param validatorFunction Checks, if the given configuration options are valid.
  Should return true, if they are valid.
  @param errorMessage A `std::runtime_error` with this as an error message will
  get thrown, if the `validatorFunction` returns false.
  @param configOptionsToBeChecked Proxies for the configuration options, who
  will be passed to the validator function as function arguments. Will keep the
  same order.
  */
  template <typename ValidatorT>
  void addOptionValidator(
      ValidatorT validatorFunction, std::string errorMessage,
      isInstantiation<ConstConfigOptionProxy> auto... configOptionsToBeChecked)
      requires(
          sizeof...(configOptionsToBeChecked) > 0 &&
          Validator<ValidatorT,
                    decltype(configOptionsToBeChecked.getConfigOption())...>) {
    addValidatorImpl(
        "addOptionValidator",
        []<typename T>(ConstConfigOptionProxy<T> opt) {
          return opt.getConfigOption();
        },
        transformValidatorIntoExceptionValidator<
            decltype(configOptionsToBeChecked.getConfigOption())...>(
            validatorFunction, std::move(errorMessage)),
        configOptionsToBeChecked...);
  }

  /*
  @brief Add a function to the configuration manager for verifying, that the
  given configuration options are valid. Will always be called after parsing.

  @param exceptionValidatorFunction Checks, if the given configuration options
  are valid. Return an empty instance of `std::optional<ErrorMessage>` if valid.
  Otherwise, the `ErrorMessage` should contain the reason, why the options are
  none valid.
  @param configOptionsToBeChecked Proxies for the configuration options, who
  will be passed to the validator function as function arguments. Will keep the
  same order.
  */
  template <typename ExceptionValidatorT>
  void addOptionValidator(
      ExceptionValidatorT exceptionValidatorFunction,
      isInstantiation<ConstConfigOptionProxy> auto... configOptionsToBeChecked)
      requires(sizeof...(configOptionsToBeChecked) > 0 &&
               ExceptionValidator<
                   ExceptionValidatorT,
                   decltype(configOptionsToBeChecked.getConfigOption())...>) {
    addValidatorImpl(
        "addOptionValidator",
        []<typename T>(ConstConfigOptionProxy<T> opt) {
          return opt.getConfigOption();
        },
        exceptionValidatorFunction, configOptionsToBeChecked...);
  }

 private:
  FRIEND_TEST(ConfigManagerTest, ParseConfig);
  FRIEND_TEST(ConfigManagerTest, ParseConfigExceptionTest);
  FRIEND_TEST(ConfigManagerTest, ParseShortHandTest);
  FRIEND_TEST(ConfigManagerTest, ContainsOption);
  FRIEND_TEST(ConfigManagerTest, CheckForBrokenPaths);

  /*
  @brief Creates the string representation of a valid `nlohmann::json` pointer
  based on the given keys.
  */
  static std::string createJsonPointerString(
      const std::vector<std::string>& keys);

  /*
  @brief Verifies, that the given path is a valid path for an option, or sub
  manager. If not, throws exceptions.

  @param pathToOption Describes a path in json.
  */
  void verifyPath(const std::vector<std::string>& path) const;

  /*
  @brief Adds a configuration option, that can be accessed with the given path.

  @param pathToOption Describes a path in json, that points to the value held by
  the configuration option.

  @return The added config option.
  */
  ConfigOption& addConfigOption(const std::vector<std::string>& pathToOption,
                                ConfigOption&& option);

  /*
  @brief Return string representation of a `std::vector<std::string>`.
  */
  static std::string vectorOfKeysForJsonToString(
      const std::vector<std::string>& keys);

  /*
  @brief Return a string, containing a list of all entries from
  `getListOfNotChangedConfigOptionsWithDefaultValues`, in the form of
  "Configuration option 'x' was not set at runtime, using default value 'y'.".
  */
  std::string getListOfNotChangedConfigOptionsWithDefaultValuesAsString() const;

  /*
  @brief Creates and adds a new configuration option.

  @tparam OptionType The type of value, the configuration option can hold.

  @param pathToOption Describes a path in json, that points to the value held by
  the configuration option. The last key in the vector is the name of the
  configuration option.
  @param optionDescription A description for the configuration option.
  @param variableToPutValueOfTheOptionIn The value held by the configuration
  option will be copied into this variable, whenever the value in the
  configuration option changes.
  @param defaultValue A default value for the configuration option. If none is
  given, signified by an empty optional, then a value for the configuration
  option MUST be given at runtime.

  @return A reference to the newly created configuration option. Will stay
  valid, even after more options.
  */
  template <typename OptionType>
  requires ad_utility::isTypeContainedIn<OptionType,
                                         ConfigOption::AvailableTypes>
  ConstConfigOptionProxy<OptionType> addOptionImpl(
      const std::vector<std::string>& pathToOption,
      std::string_view optionDescription,
      OptionType* variableToPutValueOfTheOptionIn,
      std::optional<OptionType> defaultValue =
          std::optional<OptionType>(std::nullopt)) {
    verifyPath(pathToOption);

    /*
    The `unqiue_ptr` was created, by creating a new `ConfigOption` via it's
    move constructor. Which is why, we can't just return the `ConfigOption`
    we created here.
    */
    return ConstConfigOptionProxy<OptionType>(addConfigOption(
        pathToOption,
        ConfigOption(pathToOption.back(), optionDescription,
                     variableToPutValueOfTheOptionIn, defaultValue)));
  }

  /*
  @brief A vector to all the configuratio options, held by this manager,
  represented with their json paths and reference to them. Options held by a sub
  manager, are also included with the path to the sub manager as prefix.
  */
  std::vector<std::pair<std::string, ConfigOption&>> configurationOptions();
  std::vector<std::pair<std::string, const ConfigOption&>>
  configurationOptions() const;

  /*
  @brief The implementation for `configurationOptions`.

  @tparam ReturnReference Should be either `ConfigOption&`, or `const
  ConfigOption&`.

  @param pathPrefix This prefix will be added to all configuration option json
  paths, that will be returned.
  */
  template <typename ReturnReference>
  requires std::same_as<ReturnReference, ConfigOption&> ||
           std::same_as<ReturnReference, const ConfigOption&>
  static std::vector<std::pair<std::string, ReturnReference>>
  configurationOptionsImpl(auto& configurationOptions,
                           std::string_view pathPrefix = "");

  /*
  @brief Call all the validators to check, if the current value is valid.
  */
  void verifyWithValidators() const;

  /*
  Checks, if the given configuration option is contained in the manager.
  Note: This checks for identity (pointer equality), not semantic equality.
  */
  bool containsOption(const ConfigOption& opt) const;

  /*
  @brief Transform a validator function into an exception validator function.
  That is, instead of returning `bool`, it now returns
  `std::optional<ErrorMessage>`.

  @param errorMessage Content for `ErrorMessage`.
  */
  template <typename... ValidatorParameterTypes>
  auto transformValidatorIntoExceptionValidator(
      Validator<ValidatorParameterTypes...> auto validatorFunction,
      std::string errorMessage) {
    // The whole 'transformation' is simply a wrapper.
    return [validatorFunction,
            errorMessage = ErrorMessage{std::move(errorMessage)}](
               const ValidatorParameterTypes&... args)
               -> std::optional<ErrorMessage> {
      if (std::invoke(validatorFunction, args...)) {
        return std::nullopt;
      } else {
        return errorMessage;
      }
    };
  }

  /*
  @brief The implementation of `addValidator` and `addOptionValidator`. Note:
  Normal validator functions must first be transformed in to an exception
  validator function via `transformValidatorIntoExceptionValidator`.

  @param addValidatorFunctionName The name of the function, that is calling this
  implementation. So that people can easier identify them.
  @param translationFunction Transforms the given `ConstConfigOptionProxy` in
  the value form for the exception validator function.
  @param exceptionValidatorFunction Should return an empty
  `std::optional<ErrorMessage>`, if the transformed `ConstConfigOptionProxy`
  represent valid values. Otherwise, it should return an error message.
  For example: There configuration options were all set at run time, or contain
  numbers bigger than 10.
  @param configOptionsToBeChecked Proxies for the configuration options, who
  will be passed to the exception validator function as function arguments,
  after being transformed. Will keep the same order.
  */
  template <
      typename TranslationFunction, typename ExceptionValidatorFunction,
      isInstantiation<ConstConfigOptionProxy>... ExceptionValidatorParameter>
  requires(std::invocable<TranslationFunction,
                          const ExceptionValidatorParameter> &&
           ...) &&
          ExceptionValidator<
              ExceptionValidatorFunction,
              std::invoke_result_t<TranslationFunction,
                                   ExceptionValidatorParameter>...> &&
          (sizeof...(ExceptionValidatorParameter) > 0) void addValidatorImpl(
      std::string_view addValidatorFunctionName,
      TranslationFunction translationFunction,
      ExceptionValidatorFunction exceptionValidatorFunction,
      const ExceptionValidatorParameter... configOptionsToBeChecked) {
    // Check, if we contain all the configuration options, that were given us.
    auto checkIfContainOption = [this, &addValidatorFunctionName]<typename T>(
                                    ConstConfigOptionProxy<T> opt) {
      if (!containsOption(opt.getConfigOption())) {
        throw std::runtime_error(absl::StrCat(
            "Error while adding validator with ", addValidatorFunctionName,
            ": The given configuration "
            "option '",
            opt.getConfigOption().getIdentifier(),
            "' is not contained in the configuration manager."));
      }
    };
    (checkIfContainOption(configOptionsToBeChecked), ...);

    /*
    Add a function wrapper to our list of validators, that calls the
    `exceptionValidatorFunction` with the config options and throws an
    exception, if the `exceptionValidatorFunction` returns a `ErrorMessage`.
    */
    validators_.emplace_back([translationFunction, exceptionValidatorFunction,
                              configOptionsToBeChecked...]() {
      if (const auto errorMessage = std::invoke(
              exceptionValidatorFunction,
              std::invoke(translationFunction, configOptionsToBeChecked)...);
          errorMessage.has_value()) {
        // Create the error message prefix.
        auto generateConfigOptionIdentifierWithValueString =
            [](const ConfigOption& c) {
              // Is there a value, we can show together with the identifier?
              const bool wasSet = c.wasSet();
              return absl::StrCat(
                  "'", c.getIdentifier(), "' (With ",
                  wasSet ? absl::StrCat("value '", c.getValueAsString(), "'")
                         : "no value",
                  ".)");
            };
        const std::array optionNamesWithValues{
            generateConfigOptionIdentifierWithValueString(
                configOptionsToBeChecked.getConfigOption())...};
        std::string errorMessagePrefix =
            absl::StrCat("Validity check of configuration options ",
                         lazyStrJoin(optionNamesWithValues, ", "), " failed: ");

        throw std::runtime_error(absl::StrCat(
            errorMessagePrefix, errorMessage.value().getMessage()));
      }
    });
  }
};
}  // namespace ad_utility
