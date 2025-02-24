CREATE FUNCTION dbo.IsLineReversedRobust (
    @original geometry,
    @clipped geometry,
    @tolerance float = 0.001
)
RETURNS int
AS
BEGIN
    DECLARE @orig_points TABLE (
        idx int,
        point geometry
    );
    
    DECLARE @clip_points TABLE (
        idx int,
        point geometry
    );
    
    -- 원본 라인스트링의 모든 점 추출
    DECLARE @i int = 1;
    WHILE @i <= @original.STNumPoints()
    BEGIN
        INSERT INTO @orig_points (idx, point)
        VALUES (@i, @original.STPointN(@i));
        SET @i = @i + 1;
    END;
    
    -- 잘린 라인스트링의 모든 점 추출
    SET @i = 1;
    WHILE @i <= @clipped.STNumPoints()
    BEGIN
        INSERT INTO @clip_points (idx, point)
        VALUES (@i, @clipped.STPointN(@i));
        SET @i = @i + 1;
    END;
    
    -- 첫 번째 매칭되는 점 찾기
    DECLARE @first_match TABLE (
        orig_idx int,
        clip_idx int,
        distance float
    );
    
    INSERT INTO @first_match
    SELECT TOP 1
        op.idx,
        cp.idx,
        op.point.STDistance(cp.point) as distance
    FROM @orig_points op
    CROSS APPLY (
        SELECT TOP 1 idx, point
        FROM @clip_points
        WHERE point.STDistance(op.point) <= @tolerance
        ORDER BY point.STDistance(op.point)
    ) cp
    ORDER BY distance;
    
    -- 마지막 매칭되는 점 찾기
    DECLARE @last_match TABLE (
        orig_idx int,
        clip_idx int,
        distance float
    );
    
    INSERT INTO @last_match
    SELECT TOP 1
        op.idx,
        cp.idx,
        op.point.STDistance(cp.point) as distance
    FROM @orig_points op
    CROSS APPLY (
        SELECT TOP 1 idx, point
        FROM @clip_points
        WHERE point.STDistance(op.point) <= @tolerance
        ORDER BY point.STDistance(op.point)
    ) cp
    WHERE NOT EXISTS (
        SELECT 1 FROM @first_match 
        WHERE orig_idx = op.idx OR clip_idx = cp.idx
    )
    ORDER BY distance;
    
    -- 방향성 판단
    DECLARE @first_orig_idx int, @first_clip_idx int;
    DECLARE @last_orig_idx int, @last_clip_idx int;
    
    SELECT @first_orig_idx = orig_idx, @first_clip_idx = clip_idx
    FROM @first_match;
    
    SELECT @last_orig_idx = orig_idx, @last_clip_idx = clip_idx
    FROM @last_match;
    
    -- 매칭되는 점들의 순서를 비교
    RETURN 
        CASE 
            WHEN @first_orig_idx IS NULL OR @last_orig_idx IS NULL THEN 0 -- 매칭 실패
            WHEN SIGN(@last_orig_idx - @first_orig_idx) = 
                 SIGN(@last_clip_idx - @first_clip_idx) THEN 1 -- 같은 방향
            ELSE -1 -- 반대 방향
        END;
END;

-- 사용 예시:
/*
-- 회오리 모양 테스트
DECLARE @spiral geometry = geometry::STLineFromText('LINESTRING(0 0, 1 1, 1 2, 0 2, 0 1, 0.5 1.5)', 0);
DECLARE @clipped geometry = geometry::STLineFromText('LINESTRING(0 2, 0 1, 0.5 1.5)', 0);
SELECT dbo.IsLineReversedRobust(@spiral, @clipped);
*/