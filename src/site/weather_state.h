#pragma once

namespace gs1
{
struct ForecastProfileState final
{
    std::uint32_t forecast_profile_id {0};
};

struct WeatherState final
{
    float weather_heat {0.0f};
    float weather_wind {0.0f};
    float weather_dust {0.0f};
    ForecastProfileState forecast_profile_state {};
    float site_weather_bias {0.0f};
};
}  // namespace gs1
