import fs from "fs";

const release = JSON.parse(
    fs.readFileSync(process.argv[2], "utf8")
);

function stripMarkdownImages(text)
{
    return text.replace(/!\[[^\]]*\]\([^)]+\)/g, "");
}

function parseVersion(tag)
{
    const match = tag.match(/^v?(\d+)\.(\d+)\.(\d+)/);

    if (!match)
    {
        throw new Error(
            `Invalid release tag "${tag}". Expected format: vMAJOR.MINOR.PATCH`
        );
    }

    return {
        major: Number(match[1]),
        minor: Number(match[2]),
        patch: Number(match[3])
    };
}

function detectPlatform(name)
{
    const lower = name.toLowerCase();

    if (lower.includes("windows"))
        return "Windows";

    if (lower.includes("linux"))
        return "Linux";

    if (lower.includes("macos") || lower.includes("mac"))
        return "MacOS";

    return "Other";
}

function detectTags(name)
{
    const lower = name.toLowerCase();
    const tags = [];

    if (lower.includes("x64"))
        tags.push("x64");

    if (lower.includes("arm64"))
        tags.push("arm64");

    if (lower.includes("qt"))
        tags.push("Qt");

    if (lower.includes("symbols"))
        tags.push("symbols");

    return tags;
}

const version = parseVersion(release.tag_name);

const output = {
    data: [
        {
            version: release.tag_name,
            url: release.html_url,

            semverMajor: version.major,
            semverMinor: version.minor,
            semverPatch: version.patch,

            description: stripMarkdownImages(release.body ?? "").trim(),

            assets: {},

            type: 2,

            prerelease: release.prerelease,

            createdAt: release.created_at,
            publishedAt: release.published_at
        }
    ],

    pageInfo: {
        total: 1
    }
};

for (const asset of release.assets)
{
    const platform = detectPlatform(asset.name);

    output.data[0].assets[platform] ??= [];

    output.data[0].assets[platform].push({
        url: asset.browser_download_url,

        displayName: asset.name,

        additionalTags: detectTags(asset.name),

        downloadCount: asset.download_count ?? 0,

        size: asset.size
    });
}

fs.writeFileSync(
    process.argv[3],
    JSON.stringify(output, null, 4)
);

console.log(`generated ${process.argv[3]}`);