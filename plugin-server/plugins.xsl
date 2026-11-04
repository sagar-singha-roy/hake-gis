<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="html" encoding="UTF-8" indent="yes"/>

<xsl:template match="plugins">
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Hake GeoDesk – Plugin Repository</title>
  <style type="text/css">
    :root {
      --primary: #164A73;
      --hover: #22658F;
      --active: #0E3858;
      --text: #243B53;
      --secondary: #607D94;
      --bg: #F7F9FB;
      --surface: #FFFFFF;
      --light: #EAF2F7;
      --border: #C7D8E5;
      --radius: 10px;
      --space: 8px;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: system-ui, -apple-system, "Segoe UI", Roboto, Ubuntu, Cantarell, sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.5;
    }
    a { color: var(--primary); }
    a:hover { color: var(--hover); }
    a:focus-visible {
      outline: 2px solid var(--hover);
      outline-offset: 2px;
    }
    .page {
      max-width: 1100px;
      margin: 0 auto;
      padding: calc(var(--space) * 3);
    }
    .header {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: calc(var(--space) * 3);
      margin-bottom: calc(var(--space) * 3);
      box-shadow: 0 1px 2px rgba(36, 59, 83, 0.06);
    }
    .brand-row {
      display: flex;
      flex-wrap: wrap;
      gap: calc(var(--space) * 2);
      align-items: center;
    }
    .brand-row img {
      max-height: 64px;
      width: auto;
    }
    .eyebrow {
      font-size: 0.75rem;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      color: var(--secondary);
      margin: 0 0 var(--space) 0;
      font-weight: 600;
    }
    h1 {
      margin: 0 0 var(--space) 0;
      font-size: clamp(1.35rem, 2.5vw, 1.85rem);
      color: var(--primary);
    }
    .subtitle {
      margin: 0;
      color: var(--secondary);
      font-size: 1rem;
    }
    .meta {
      margin-top: calc(var(--space) * 2);
      padding-top: calc(var(--space) * 2);
      border-top: 1px solid var(--border);
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: var(--space);
      font-size: 0.9rem;
    }
    .meta dt {
      color: var(--secondary);
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.04em;
      margin: 0;
    }
    .meta dd {
      margin: 2px 0 0 0;
      font-weight: 600;
    }
    .intro {
      color: var(--secondary);
      margin: 0 0 calc(var(--space) * 3) 0;
      max-width: 42rem;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
      gap: calc(var(--space) * 2);
    }
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: calc(var(--space) * 2.5);
      display: flex;
      flex-direction: column;
      gap: calc(var(--space) * 1.5);
      box-shadow: 0 1px 2px rgba(36, 59, 83, 0.05);
    }
    .card h2 {
      margin: 0;
      font-size: 1.1rem;
      color: var(--text);
    }
    .version {
      display: inline-block;
      background: var(--light);
      color: var(--primary);
      font-size: 0.8rem;
      font-weight: 600;
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid var(--border);
    }
    .card p {
      margin: 0;
      font-size: 0.92rem;
      color: var(--text);
    }
    .muted { color: var(--secondary); font-size: 0.85rem; }
    .tags {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
    }
    .tag {
      background: var(--light);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 2px 8px;
      font-size: 0.75rem;
      color: var(--secondary);
    }
    .status {
      font-size: 0.8rem;
      font-weight: 600;
    }
    .status.ok { color: var(--primary); }
    .status.warn { color: #9a5b00; }
    .links {
      display: flex;
      flex-wrap: wrap;
      gap: calc(var(--space) * 1.5);
      font-size: 0.85rem;
    }
    .filename {
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 0.75rem;
      color: var(--secondary);
      word-break: break-all;
    }
    .actions { margin-top: auto; padding-top: var(--space); }
    .btn {
      display: inline-block;
      background: var(--primary);
      color: #fff !important;
      text-decoration: none;
      font-weight: 600;
      font-size: 0.9rem;
      padding: 10px 16px;
      border-radius: 8px;
      border: none;
    }
    .btn:hover { background: var(--hover); }
    .btn:active { background: var(--active); }
    .btn:focus-visible {
      outline: 2px solid var(--hover);
      outline-offset: 2px;
    }
    .footer {
      margin-top: calc(var(--space) * 4);
      padding: calc(var(--space) * 2);
      text-align: center;
      color: var(--secondary);
      font-size: 0.85rem;
    }
    @media (max-width: 600px) {
      .page { padding: calc(var(--space) * 2); }
      .brand-row { flex-direction: column; align-items: flex-start; }
    }
  </style>
</head>
<body>
  <div class="page">
    <header class="header">
      <div class="brand-row">
        <img src="logo.png" alt="Hake Geospatial logo"/>
        <div>
          <p class="eyebrow">HAKE GEOSPATIAL</p>
          <h1>Hake GeoDesk – Desktop GIS</h1>
          <p class="subtitle">Python Plugin Repository</p>
        </div>
      </div>
      <dl class="meta">
        <div>
          <dt>Version filter</dt>
          <dd>Hake GeoDesk 2026</dd>
        </div>
        <div>
          <dt>Product version</dt>
          <dd>2026.0.1</dd>
        </div>
        <div>
          <dt>Company</dt>
          <dd>Hake Technologies</dd>
        </div>
      </dl>
    </header>

    <p class="intro">
      Plugin repository for Hake GeoDesk – Desktop GIS.
      Professional GIS for Mapping, Analysis &amp; Spatial Intelligence.
      Install plugins from Hake GeoDesk via Plugin Manager, or download packages below.
    </p>

    <main class="grid">
      <xsl:for-each select="pyqgis_plugin">
        <xsl:sort select="@name"/>
        <article class="card">
          <div>
            <h2><xsl:value-of select="@name"/></h2>
            <xsl:if test="@version != ''">
              <span class="version">v<xsl:value-of select="@version"/></span>
            </xsl:if>
          </div>

          <xsl:if test="description != ''">
            <p><xsl:value-of select="description"/></p>
          </xsl:if>
          <xsl:if test="about != ''">
            <p class="muted"><xsl:value-of select="about"/></p>
          </xsl:if>

          <xsl:if test="tags != ''">
            <div class="tags">
              <xsl:call-template name="split-tags">
                <xsl:with-param name="list" select="tags"/>
              </xsl:call-template>
            </div>
          </xsl:if>

          <xsl:if test="author_name != ''">
            <p class="muted">Author: <xsl:value-of select="author_name"/></p>
          </xsl:if>

          <xsl:if test="qgis_minimum_version != '' or qgis_maximum_version != ''">
            <p class="muted">
              Compatible version range (upstream metadata):
              <xsl:value-of select="qgis_minimum_version"/>
              <xsl:if test="qgis_maximum_version != ''">
                – <xsl:value-of select="qgis_maximum_version"/>
              </xsl:if>
            </p>
          </xsl:if>

          <div>
            <xsl:choose>
              <xsl:when test="deprecated = 'True' or deprecated = 'true'">
                <span class="status warn">Deprecated</span>
              </xsl:when>
              <xsl:when test="experimental = 'True' or experimental = 'true'">
                <span class="status warn">Experimental</span>
              </xsl:when>
              <xsl:when test="trusted = 'True' or trusted = 'true'">
                <span class="status ok">Trusted</span>
              </xsl:when>
            </xsl:choose>
          </div>

          <div class="links">
            <xsl:if test="homepage != ''">
              <a href="{homepage}">Homepage</a>
            </xsl:if>
            <xsl:if test="tracker != ''">
              <a href="{tracker}">Tracker</a>
            </xsl:if>
            <xsl:if test="repository != ''">
              <a href="{repository}">Source</a>
            </xsl:if>
          </div>

          <xsl:if test="file_name != ''">
            <div class="filename"><xsl:value-of select="file_name"/></div>
          </xsl:if>

          <xsl:if test="download_url != ''">
            <div class="actions">
              <a class="btn" href="{download_url}">Download</a>
            </div>
          </xsl:if>
        </article>
      </xsl:for-each>
    </main>

    <footer class="footer">
      Powered by Hake Technologies · HAKE GEOSPATIAL
    </footer>
  </div>
</body>
</html>
</xsl:template>

<!-- Simple comma-separated tag splitter -->
<xsl:template name="split-tags">
  <xsl:param name="list"/>
  <xsl:choose>
    <xsl:when test="contains($list, ',')">
      <span class="tag"><xsl:value-of select="normalize-space(substring-before($list, ','))"/></span>
      <xsl:call-template name="split-tags">
        <xsl:with-param name="list" select="substring-after($list, ',')"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="normalize-space($list) != ''">
      <span class="tag"><xsl:value-of select="normalize-space($list)"/></span>
    </xsl:when>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
