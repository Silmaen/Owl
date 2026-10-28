-- Editor script: plants a row of five markers along x, one undo step for the whole row.
-- Run it from the Console (Editor Scripts menu) or from its context menu in the Content Browser.
local count = 5
local spacing = 1.5
for i = 1, count do
	local marker = editor.entity.create{name = "Marker " .. i}
	editor.entity.set_transform{entity = marker.entity, translation = {(i - 1) * spacing, 0, 0}}
end
print(string.format("Planted %d markers, %.1f apart.", count, spacing))
