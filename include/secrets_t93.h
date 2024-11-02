#define SECRET_WIFI_PASSWORD "CxCtEsp123" // No more than 10 characters.
#define SECRET_API_ENDPOINT "https://dev-apigateway.vocusgroup.co.nz/mobile/v2/metadata/displayValue"
#define SECRET_API_KEY "927661c9b0e340338e0b15db835d3124"

const char _valueLabel[3][17] = {
  "Active Prepay:",
  "Active Postpay:",
  "Active Tahi:"
};

const char _valueSelectionSummary[3][2][17] = {
  { "Total active 2d", "prepay subs" },
  { "Total active 2d", "postpay subs" },
  { "Total active 2d", "Tahi subs" }
};
