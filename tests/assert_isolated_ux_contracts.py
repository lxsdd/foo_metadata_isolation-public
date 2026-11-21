"""Independent real foobar logging redaction and native hover-tooltip contract oracle.

These probes use synthetic credentials and a disposable native UI, not user data.
"""
import argparse
from pathlib import Path
import re

def read_items(report):
    values={}
    for line in report.read_text(encoding="utf-8-sig").splitlines():
        if "=" in line:
            k,v=line.split("=",1)
            if k not in values: values[k]=v
    return values

def require(items,k,v):
    assert items.get(k)==v, f"{k}: expected {v!r}, got {items.get(k)!r}"

def main():
    p=argparse.ArgumentParser()
    p.add_argument("report",type=Path)
    p.add_argument("--scenario",choices=["logging","tooltip-light","tooltip-dark"],required=True)
    a=p.parse_args()
    data=read_items(a.report)
    if a.scenario=="logging":
        require(data,"write_result","logging_contract_complete")
        require(data,"credential_sentinels_absent","true")
        require(data,"redaction_count","4")
        require(data,"unicode_preserved","true")
        require(data,"session_prefix_present","true")
        line=data.get("logging_line","")
        assert "<redacted>" in line
        for secret in ("ACCESS_TOKEN_SENTINEL_DO_NOT_LOG","REFRESH_TOKEN_SENTINEL_DO_NOT_LOG",
                       "PASSWORD_SENTINEL_DO_NOT_LOG","COOKIE_SENTINEL_DO_NOT_LOG"):
            assert secret not in a.report.read_text(encoding="utf-8-sig"), "secret exposed in report"
        print("REAL_HOST_LOGGING_REDACTION_UNICODE=PASS")
        return
    require(data,"write_result","tooltip_contract_complete")
    requested=int(data.get("tooltip_requested_count","0"))
    registered=int(data.get("tooltip_registered_count","-1"))
    assert requested>0 and registered==requested, "tooltip registration mismatch"
    for key in ("tooltip_texts_match","tooltip_distinct_mapping","tooltip_hover_appeared",
                "tooltip_hover_dismissed","tooltip_unicode_roundtrip"):
        require(data,key,"true")
    require(data,"tooltip_dark_mode_requested","true" if a.scenario=="tooltip-dark" else "false")
    dpi=int(data.get("tooltip_dpi","0"))
    assert 96<=dpi<=384,"missing or invalid runtime DPI"
    print("REAL_HOST_"+a.scenario.upper().replace("-","_")+"=PASS; hover appeared/dismissed, all controls, Unicode, DPI")

if __name__=="__main__":
    main()
