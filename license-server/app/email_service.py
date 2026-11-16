"""
Email Notification Service for Hake GeoDesk Licensing Server.
Dispatches enterprise-grade HTML emails with 16-character license activation codes,
receipt summaries, and step-by-step activation guides.
Includes local spool directory (sent_emails/) for zero-config offline testing,
plus optional SMTP relay support.
"""

import os
import json
import smtplib
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from datetime import datetime, timezone
from typing import Dict, Any, List, Optional

SPOOL_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "sent_emails")

SMTP_HOST = os.environ.get("SMTP_HOST")
SMTP_PORT = int(os.environ.get("SMTP_PORT", "587"))
SMTP_USER = os.environ.get("SMTP_USER")
SMTP_PASS = os.environ.get("SMTP_PASS")
SMTP_FROM = os.environ.get("SMTP_FROM", "licenses@haketech.com")

def ensure_spool_dir():
    os.makedirs(SPOOL_DIR, exist_ok=True)

def generate_license_email_html(customer_name: str, activation_code: str, plan_tier: str, expires_at: str) -> str:
    plan_display = "Annual Pro (1 Workstation)" if plan_tier == "annual" else ("Business Team (Enterprise)" if plan_tier == "business" else "7-Day Free Trial")
    exp_display = expires_at[:10] if expires_at else "Perpetual"

    return f"""<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Your Hake GeoDesk License Activation Code</title>
  <style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: #0b1120; margin: 0; padding: 24px; color: #f8fafc; }}
    .container {{ max-width: 600px; margin: 0 auto; background: #131c2e; border: 1px solid #1e293b; border-radius: 12px; overflow: hidden; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }}
    .header {{ background: linear-gradient(135deg, #0284c7, #2563eb); padding: 32px 24px; text-align: center; color: white; }}
    .header h1 {{ margin: 0 0 6px 0; font-size: 24px; font-weight: 700; }}
    .header p {{ margin: 0; font-size: 14px; opacity: 0.9; }}
    .content {{ padding: 28px 24px; }}
    .code-box {{ background: #030712; border: 2px dashed #38bdf8; border-radius: 8px; padding: 18px; text-align: center; margin: 24px 0; }}
    .code-label {{ font-size: 11px; text-transform: uppercase; letter-spacing: 1.5px; color: #94a3b8; margin-bottom: 8px; }}
    .code-value {{ font-family: 'SF Mono', Consolas, Monaco, monospace; font-size: 24px; font-weight: bold; color: #38bdf8; letter-spacing: 3px; }}
    .details-table {{ width: 100%; border-collapse: collapse; margin-bottom: 24px; }}
    .details-table td {{ padding: 10px 0; border-bottom: 1px solid #1e293b; font-size: 13px; }}
    .details-table td.label {{ color: #94a3b8; width: 40%; }}
    .details-table td.val {{ font-weight: 600; color: #f1f5f9; }}
    .steps {{ background: #0b1120; border-radius: 8px; padding: 16px; margin-bottom: 24px; font-size: 13px; color: #cbd5e1; line-height: 1.6; }}
    .steps ol {{ margin: 0; padding-left: 20px; }}
    .steps li {{ margin-bottom: 6px; }}
    .footer {{ background: #0b1120; padding: 20px 24px; text-align: center; font-size: 11px; color: #64748b; border-top: 1px solid #1e293b; }}
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>Hake GeoDesk</h1>
      <p>Professional Enterprise GIS Platform</p>
    </div>
    <div class="content">
      <p style="font-size: 15px; margin-top: 0;">Hello <strong>{customer_name}</strong>,</p>
      <p style="font-size: 13px; color: #cbd5e1;">Thank you for choosing Hake GeoDesk! Your license has been provisioned and is ready for workstation activation.</p>

      <div class="code-box">
        <div class="code-label">Your 16-Character Activation Code</div>
        <div class="code-value">{activation_code}</div>
      </div>

      <table class="details-table">
        <tr>
          <td class="label">Subscription Plan:</td>
          <td class="val">{plan_display}</td>
        </tr>
        <tr>
          <td class="label">Active Until:</td>
          <td class="val">{exp_display}</td>
        </tr>
        <tr>
          <td class="label">Workstation Seats:</td>
          <td class="val">1 Dedicated Device (Transferable)</td>
        </tr>
      </table>

      <div class="steps">
        <strong>How to activate your desktop workstation:</strong>
        <ol>
          <li>Launch <strong>Hake GeoDesk</strong> on your desktop.</li>
          <li>On the licensing welcome screen, click <strong>"I Have an Activation Code"</strong>.</li>
          <li>Copy and paste your 16-character code above and click <strong>Activate License</strong>.</li>
          <li>Your workstation will be verified immediately and unlock full GIS capabilities.</li>
        </ol>
      </div>

      <p style="font-size: 12px; color: #94a3b8;">Need assistance or workstation migration support? Contact us at <a href="mailto:support@haketech.com" style="color: #38bdf8;">support@haketech.com</a>.</p>
    </div>
    <div class="footer">
      © 2026 Hake Technologies Inc. All rights reserved.
    </div>
  </div>
</body>
</html>
"""

def send_license_email(
    to_email: str,
    customer_name: str,
    activation_code: str,
    plan_tier: str,
    expires_at: str
) -> Dict[str, Any]:
    """
    Sends license delivery email. Always persists to local spool,
    and relays via SMTP if configured.
    """
    ensure_spool_dir()
    subject = f"Your Hake GeoDesk License Code: {activation_code}"
    html_content = generate_license_email_html(customer_name, activation_code, plan_tier, expires_at)
    plain_content = f"""Hello {customer_name},

Thank you for choosing Hake GeoDesk.
Your 16-character activation code is: {activation_code}

Plan: {plan_tier.upper()}
Expires: {expires_at}

To activate: Launch Hake GeoDesk -> click "I Have an Activation Code" -> enter {activation_code}.
Support: support@haketech.com
"""

    record = {
        "to_email": to_email,
        "customer_name": customer_name,
        "activation_code": activation_code,
        "plan_tier": plan_tier,
        "expires_at": expires_at,
        "subject": subject,
        "sent_at": datetime.now(timezone.utc).isoformat(),
        "html": html_content,
        "plain": plain_content,
        "relay_status": "spooled_locally"
    }

    # Relay via SMTP if configured
    if SMTP_HOST:
        try:
            msg = MIMEMultipart("alternative")
            msg["Subject"] = subject
            msg["From"] = SMTP_FROM
            msg["To"] = to_email
            msg.attach(MIMEText(plain_content, "plain"))
            msg.attach(MIMEText(html_content, "html"))

            with smtplib.SMTP(SMTP_HOST, SMTP_PORT, timeout=10) as server:
                if SMTP_USER and SMTP_PASS:
                    server.starttls()
                    server.login(SMTP_USER, SMTP_PASS)
                server.sendmail(SMTP_FROM, [to_email], msg.as_string())
            record["relay_status"] = "sent_via_smtp"
        except Exception as e:
            record["relay_status"] = f"smtp_failed: {e}"

    # Always persist to spool file for inspection and local development
    filename = f"{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}_{to_email.replace('@', '_at_')}.json"
    filepath = os.path.join(SPOOL_DIR, filename)
    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(record, f, indent=2)

    return record

def list_spooled_emails(limit: int = 20) -> List[Dict[str, Any]]:
    """Returns recent sent emails from local spool."""
    ensure_spool_dir()
    files = sorted(os.listdir(SPOOL_DIR), reverse=True)
    results = []
    for fn in files[:limit]:
        if fn.endswith(".json"):
            fp = os.path.join(SPOOL_DIR, fn)
            try:
                with open(fp, "r", encoding="utf-8") as f:
                    results.append(json.load(f))
            except Exception:
                pass
    return results
