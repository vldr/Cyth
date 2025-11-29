import "env"
  void log(string n)
  void log(int n)

string urlify(string input)
  char[] buffer

  for char c in input 
    if c >= 'A' and c <= 'Z'
      c = (char)(c - 'A' + 'a')

    if (c >= 'a' and c <= 'z') or (c >= '0' and c <= '9')
      buffer.push(c)
    else if (
      buffer.length > 0 and 
      it < input.length - 1 and
      buffer[buffer.length - 1] != '-' and 
      (
        (input[it + 1] >= 'A' and input[it + 1] <= 'Z') or 
        (input[it + 1] >= 'a' and input[it + 1] <= 'z') or
        (input[it + 1] >= '0' and input[it + 1] <= '9') 
      )
    )
      buffer.push('-')

  return buffer.toString()


log(urlify("      AI Cloud--flare Week 2025:      Recap f......f.f            ---"))
log(urlify("AI Week 2025: Recap"))
log(urlify("Cloudy     Summarizations of Email Detections: Beta Announcement"))
log(urlify("The crawl-to-click gap: Cloudflare data on AI bots, training, and referrals"))
log(urlify("Cloudflare incident on August 21, 2025"))
log(urlify("MadeYouReset: An HTTP/2 vulnerability thwarted by Rapid Reset mitigations"))
log(urlify("--------------------"))

# ai-cloud-flare-week-2025-recap-f-f-f
# ai-week-2025-recap
# cloudy-summarizations-of-email-detections-beta-announcement
# the-crawl-to-click-gap-cloudflare-data-on-ai-bots-training-and-referrals
# cloudflare-incident-on-august-21-2025
# madeyoureset-an-http-2-vulnerability-thwarted-by-rapid-reset-mitigations
#